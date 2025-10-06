#ifndef MESHWARP_VIEWER_H
#define MESHWARP_VIEWER_H

#include <OpenXRApp.h>

#include <Primitives/Mesh.h>
#include <Primitives/Model.h>
#include <Materials/UnlitMaterial.h>
#include <Lights/AmbientLight.h>
#include <PostProcessing/Tonemapper.h>

#include <Buffer.h>
#include <Cameras/PerspectiveCamera.h>
#include <Utils/FileIO.h>

#include <Receivers/MeshWarpReceiver.h>

using namespace quasar;

class MeshWarpViewer final : public OpenXRApp {
private:
    std::string sceneName = "robot_lab"; // choose from robot_lab, sun_temple, viking_village, or san_miguel
    Path dataPath = Path("meshwarp/" + sceneName + "/");

    const glm::uvec2 remoteGBufferSize{3840, 2160};

    uint vertexGroupSize = 4;
    uint depthFactor = 1;
    float remoteFOV = 120.0f;

public:
    MeshWarpViewer() = default;
    ~MeshWarpViewer() = default;

private:
    void CreateResources() override {
        scene->setAmbientLight(new AmbientLight({ .intensity = 1.0f }));

        // Add the hand nodes
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

        tonemapper = std::make_unique<Tonemapper>(false);

        // Create MeshWarpReceiver and load from disk
        meshWarpReceiver = std::make_unique<MeshWarpReceiver>(remoteGBufferSize, depthFactor, vertexGroupSize, remoteFOV);

        // Attach receiver mesh to nodes
        node.setEntity(&meshWarpReceiver->getMesh());
        node.frustumCulled = false;
        scene->addChildNode(&node);

        nodeWireframe.setEntity(&meshWarpReceiver->getMesh());
        nodeWireframe.frustumCulled = false;
        nodeWireframe.wireframe = true;
        nodeWireframe.visible = false;
        nodeWireframe.primitiveType = GL_LINES;
        nodeWireframe.overrideMaterial = new UnlitMaterial({ .baseColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) });
        scene->addChildNode(&nodeWireframe);

        // Load data from files
        meshWarpReceiver->loadFromFiles(dataPath);
        auto& remoteCamera = meshWarpReceiver->getRemoteCamera();
        cameraPositionOffset = remoteCamera.getPosition();
    }

    void CreateActionSet() override {
        // An Action for clicking on the controller
        CreateAction(clickAction, "click-controller", XR_ACTION_TYPE_BOOLEAN_INPUT, {"/user/hand/left", "/user/hand/right"});
        // An Action for the position of the thumbstick
        CreateAction(thumbstickAction, "thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, {"/user/hand/left", "/user/hand/right"});
        // An Action for a vibration output on one or other hand
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
            if (buzz[i] < 0.01f) {
                buzz[i] = 0.0f;
            }
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

    void HandleInteractions(double now, double dt) override {
        // For each hand:
        for (int i = 0; i < 2; i++) {
            // Draw the controllers:
            handNodes[i].visible = handPoseState[i].isActive;

            if (clickState[i].isActive == XR_TRUE &&
                clickState[i].currentState == XR_FALSE &&
                clickState[i].changedSinceLastSync == XR_TRUE) {
                // XR_LOG("Click action triggered for hand: " << i);
                buzz[i] = 0.5f;

                nodeWireframe.visible = !nodeWireframe.visible;
            }

            if (thumbstickState[i].isActive == XR_TRUE && thumbstickState[i].changedSinceLastSync == XR_TRUE) {
                if (glm::abs(thumbstickState[i].currentState.x) > 0.2f || glm::abs(thumbstickState[i].currentState.y) > 0.2f) {
                    const glm::vec3& forward = cameras->left.getForwardVector();
                    const glm::vec3& right = cameras->left.getRightVector();
                    cameraPositionOffset += movementSpeed * forward * thumbstickState[i].currentState.y * static_cast<float>(dt);
                    cameraPositionOffset += movementSpeed *   right * thumbstickState[i].currentState.x * static_cast<float>(dt);
                }
                // XR_LOG("Thumbstick action triggered for hand: " << i << " with value: " << thumbstickState[i].currentState.x << ", " << thumbstickState[i].currentState.y);
            }
        }
    }

    void OnRender(double now, double dt) override {
        // Mesh is generated/loaded by MeshWarpReceiver; just render the scene
        renderer->drawObjects(*scene, *cameras);
        tonemapper->drawToScreen(*renderer);
        // spdlog::info("Total Frame time: {:.3f}ms", timeutils::secondsToMillis(dt));
    }

    void DestroyResources() override {}

private:
    std::unique_ptr<Tonemapper> tonemapper;

    ZSTDCodec codec;
    std::unique_ptr<Texture> colorTexture;
    Node node, nodeWireframe;

    std::unique_ptr<MeshWarpReceiver> meshWarpReceiver;

    // Actions
    XrAction clickAction;
    // The realtime states of these actions
    XrActionStateBoolean clickState[2] = {{XR_TYPE_ACTION_STATE_BOOLEAN}, {XR_TYPE_ACTION_STATE_BOOLEAN}};
    // The thumbstick input action
    XrAction thumbstickAction;
    // The current thumbstick state for each controller
    XrActionStateVector2f thumbstickState[2] = {{XR_TYPE_ACTION_STATE_VECTOR2F}, {XR_TYPE_ACTION_STATE_VECTOR2F}};
    float movementSpeed = 2.0f;
    // The haptic output action for grabbing
    XrAction buzzAction;
    // The current haptic output value for each controller
    float buzz[2] = {0, 0};

    std::unique_ptr<Model> handModelLeft;
    std::unique_ptr<Model> handModelRight;
};

#endif // MESHWARP_VIEWER_H
