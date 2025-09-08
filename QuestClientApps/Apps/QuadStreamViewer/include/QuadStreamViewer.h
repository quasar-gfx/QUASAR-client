#ifndef QUAD_STREAM_RECEIVER_H
#define QUAD_STREAM_RECEIVER_H

#include <OpenXRApp.h>

#include <Path.h>
#include <Primitives/Mesh.h>
#include <Primitives/Model.h>
#include <Lights/AmbientLight.h>

#include <Receivers/QuadStreamReceiver.h>

using namespace quasar;

class QuadStreamViewer final : public OpenXRApp {
private:
    std::string sceneName = "robot_lab"; // choose from robot_lab, sun_temple, viking_village, or san_miguel
    Path dataPath = Path("quads/" + sceneName + "/");

    uint maxAdditionalViews = 8;
    uint maxViews = maxAdditionalViews + 2; // +2 for primary and wide fov views

    const glm::uvec2 remoteGBufferSize = glm::uvec2(1920, 1080);

public:
    QuadStreamViewer(GraphicsAPI_Type apiType)
        : OpenXRApp(apiType)
    {}
    ~QuadStreamViewer() = default;

private:
    void CreateResources() override {
        scene->setAmbientLight(new AmbientLight({ .intensity = 1.0f }));

        // Add controller models
        handModelLeft = std::make_unique<Model>(ModelCreateParams{
            .flipTextures = true,
            .gammaCorrected = true,
            .path = "models/quest-touch-plus-left.glb"
        });
        handNodes[0].setPosition({ 0.0065f, -0.008f, -0.04f });
        handNodes[0].setRotationEuler({ -16.0f, 0.0f, 0.0f });
        handNodes[0].setEntity(handModelLeft.get());

        handModelRight = std::make_unique<Model>(ModelCreateParams{
            .flipTextures = true,
            .gammaCorrected = true,
            .path = "models/quest-touch-plus-right.glb"
        });
        handNodes[1].setPosition({ -0.0065f, -0.008f, -0.04f });
        handNodes[1].setRotationEuler({ -16.0f, 0.0f, 0.0f });
        handNodes[1].setEntity(handModelRight.get());

        quadSet = std::make_unique<QuadSet>(remoteGBufferSize);
        quadstreamReceiver = std::make_unique<QuadStreamReceiver>(*quadSet, maxViews);

        // Create nodes
        nodes.reserve(maxViews);
        nodeWireframes.reserve(maxViews);
        for (int view = 0; view < maxViews; view++) {
            nodes.emplace_back(&quadstreamReceiver->getMesh(view));
            nodes[view].frustumCulled = false;
            scene->addChildNode(&nodes[view]);

            nodeWireframes.emplace_back(&quadstreamReceiver->getMesh(view));
            nodeWireframes[view].frustumCulled = false;
            nodeWireframes[view].wireframe = true;
            nodeWireframes[view].visible = false;
            nodeWireframes[view].primitiveType = GL_LINES;
            nodeWireframes[view].overrideMaterial = new QuadMaterial({ .baseColor = colors[view % colors.size()] });
            scene->addChildNode(&nodeWireframes[view]);
        }

        // Load quad buffers and depth offsets
        quadstreamReceiver->loadFromFiles(dataPath);
        auto& remoteCamera = quadstreamReceiver->getRemoteCamera();
        cameraPositionOffset = remoteCamera.getPosition();

        spdlog::info("Time to load: {:.3f}ms", quadstreamReceiver->stats.loadTimeMs);
        spdlog::info("Time to decompress: {:.3f}ms", quadstreamReceiver->stats.decompressTimeMs);
        spdlog::info("Time to transfer to GPU: {:.3f}ms", quadstreamReceiver->stats.transferTimeMs);
        spdlog::info("Time to create mesh: {:.3f}ms", quadstreamReceiver->stats.createMeshTimeMs);
        spdlog::info("Loaded {} quads ({:.3f} MB), {} depth offsets ({:.3f} MB)",
                     quadstreamReceiver->stats.sizes.numQuads, quadstreamReceiver->stats.sizes.quadsSize / BYTES_PER_MEGABYTE,
                     quadstreamReceiver->stats.sizes.numDepthOffsets, quadstreamReceiver->stats.sizes.depthOffsetsSize / BYTES_PER_MEGABYTE);
    }

    void CreateActionSet() override {
        // An Action for clicking on the controller.
        CreateAction(clickAction, "click-controller", XR_ACTION_TYPE_BOOLEAN_INPUT, {"/user/hand/left", "/user/hand/right"});
        // An Action for the position of the thumbstick.
        CreateAction(thumbstickAction, "thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, {"/user/hand/left", "/user/hand/right"});
        // An Action for a vibration output on one or other hand.
        CreateAction(buzzAction, "buzz", XR_ACTION_TYPE_VIBRATION_OUTPUT, {"/user/hand/left", "/user/hand/right"});
    }

    void SuggestBindings(std::map<std::string, std::vector<XrActionSuggestedBinding>>& bindings) override {
        bindings["/interaction_profiles/khr/simple_controller"].push_back({clickAction, CreateXrPath("/user/hand/left/input/select/click")});
        bindings["/interaction_profiles/khr/simple_controller"].push_back({clickAction, CreateXrPath("/user/hand/right/input/select/click")});
        bindings["/interaction_profiles/khr/simple_controller"].push_back({buzzAction, CreateXrPath("/user/hand/left/output/haptic")});
        bindings["/interaction_profiles/khr/simple_controller"].push_back({buzzAction, CreateXrPath("/user/hand/right/output/haptic")});

        bindings["/interaction_profiles/oculus/touch_controller"].push_back({clickAction, CreateXrPath("/user/hand/left/input/trigger/value")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({clickAction, CreateXrPath("/user/hand/right/input/trigger/value")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({thumbstickAction, CreateXrPath("/user/hand/left/input/thumbstick")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({thumbstickAction, CreateXrPath("/user/hand/right/input/thumbstick")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({buzzAction, CreateXrPath("/user/hand/left/output/haptic")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({buzzAction, CreateXrPath("/user/hand/right/output/haptic")});
    }

    void PollActions(XrTime predictedTime) override {
        XrActionStateGetInfo actionStateGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};

        for (int i = 0; i < 2; i++) {
            actionStateGetInfo.action = clickAction;
            actionStateGetInfo.subactionPath = handPaths[i];
            OPENXR_CHECK(xrGetActionStateBoolean(session, &actionStateGetInfo, &clickState[i]),
                        "Failed to get Boolean State of Click action.");

            actionStateGetInfo.action = thumbstickAction;
            actionStateGetInfo.subactionPath = handPaths[i];
            OPENXR_CHECK(xrGetActionStateVector2f(session, &actionStateGetInfo, &thumbstickState[i]),
                        "Failed to get Vector2f State of Thumbstick action.");

            buzz[i] *= 0.5f;
            if (buzz[i] < 0.01f)
                buzz[i] = 0.0f;
            XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
            vibration.amplitude = buzz[i];
            vibration.duration = XR_MIN_HAPTIC_DURATION;
            vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

            XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
            hapticActionInfo.action = buzzAction;
            hapticActionInfo.subactionPath = handPaths[i];
            OPENXR_CHECK(xrApplyHapticFeedback(session, &hapticActionInfo, (XrHapticBaseHeader*)&vibration),
                        "Failed to apply haptic feedback.");
        }
    }

    void HandleInteractions() override {
        // For each hand:
        for (int i = 0; i < 2; i++) {
            // Draw the controllers:
            handNodes[i].visible = handPoseState[i].isActive;

            if (clickState[i].isActive == XR_TRUE &&
                clickState[i].currentState == XR_FALSE &&
                clickState[i].changedSinceLastSync == XR_TRUE) {
                // XR_LOG("Click action triggered for hand: " << i);
                buzz[i] = 0.5f;

                for (int view = 0; view < maxViews; view++) {
                    nodeWireframes[view].visible = !nodeWireframes[view].visible;
                }
            }

            if (thumbstickState[i].isActive == XR_TRUE &&
                thumbstickState[i].changedSinceLastSync == XR_TRUE) {
                if (glm::abs(thumbstickState[i].currentState.x) > 0.2f ||
                    glm::abs(thumbstickState[i].currentState.y) > 0.2f) {
                    const glm::vec3 &forward = cameras->left.getForwardVector();
                    const glm::vec3 &right = cameras->left.getRightVector();
                    cameraPositionOffset += movementSpeed * forward * thumbstickState[i].currentState.y;
                    cameraPositionOffset += movementSpeed * right * thumbstickState[i].currentState.x;
                }
            }
        }
    }

    void OnRender(double now, double dt) override {
        graphicsAPI->drawObjects(*scene, *cameras);
        // spdlog::info("Total Frame time: {:.3f}ms", timeutils::secondsToMillis(dt));
    }

    void DestroyResources() override {}

private:
    const std::vector<glm::vec4> colors = {
        glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), // primary view color is yellow
        glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
        glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
        glm::vec4(1.0f, 0.5f, 0.5f, 1.0f),
        glm::vec4(0.0f, 0.5f, 0.5f, 1.0f),
        glm::vec4(0.5f, 0.0f, 0.0f, 1.0f),
        glm::vec4(0.0f, 1.0f, 1.0f, 1.0f),
        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
        glm::vec4(0.0f, 0.5f, 0.0f, 1.0f),
        glm::vec4(0.5f, 0.0f, 0.5f, 1.0f),
    };

    std::unique_ptr<QuadSet> quadSet;
    std::unique_ptr<QuadStreamReceiver> quadstreamReceiver;

    std::vector<Node> nodes, nodeWireframes;

    // Actions.
    XrAction clickAction;
    // The realtime states of these actions.
    XrActionStateBoolean clickState[2] = {{XR_TYPE_ACTION_STATE_BOOLEAN}, {XR_TYPE_ACTION_STATE_BOOLEAN}};
    // The thumbstick input action.
    XrAction thumbstickAction;
    // The current thumbstick state for each controller.
    XrActionStateVector2f thumbstickState[2] = {{XR_TYPE_ACTION_STATE_VECTOR2F}, {XR_TYPE_ACTION_STATE_VECTOR2F}};
    float movementSpeed = 0.03f;
    // The haptic output action for grabbing cubes.
    XrAction buzzAction;
    // The current haptic output value for each controller.
    float buzz[2] = {0, 0};

    std::unique_ptr<Model> handModelLeft;
    std::unique_ptr<Model> handModelRight;
};

#endif // QUAD_STREAM_RECEIVER_H
