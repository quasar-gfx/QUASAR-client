#ifndef QUADS_VIEWER_H
#define QUADS_VIEWER_H

#include <memory>

#include <OpenXRApp.h>

#include <Path.h>
#include <Primitives/Mesh.h>
#include <Primitives/Model.h>

#include <Lights/AmbientLight.h>

#include <Quads/QuadFrames.h>
#include <Quads/QuadMesh.h>

using namespace quasar;

class QuadsViewer final : public OpenXRApp {
private:
    std::string sceneName = "robot_lab"; // choose from robot_lab, sun_temple, viking_village, or san_miguel
    Path dataPath = Path("quads/" + sceneName + "/");

public:
    QuadsViewer(GraphicsAPI_Type apiType)
        : OpenXRApp(apiType)
    {}
    ~QuadsViewer() = default;

private:
    void CreateResources() override {
        scene->backgroundColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        AmbientLight* ambientLight = new AmbientLight({
            .intensity = 1.0f
        });
        scene->setAmbientLight(ambientLight);

        // Add the hand nodes.
        leftControllerModel = std::make_unique<Model>(ModelCreateParams{
            .flipTextures = true,
            .IBL = 0.0f,
            .path = "models/quest-touch-plus-left.glb"
        });
        handNodes[0].setEntity(leftControllerModel.get());

        rightControllerModel = std::make_unique<Model>(ModelCreateParams{
            .flipTextures = true,
            .IBL = 0.0f,
            .path = "models/quest-touch-plus-right.glb"
        });
        handNodes[1].setEntity(rightControllerModel.get());

        std::string colorFileName = dataPath.appendToName("color").withExtension(".jpg");
        colorTexture = new Texture({
            .wrapS = GL_REPEAT,
            .wrapT = GL_REPEAT,
            .minFilter = GL_NEAREST,
            .magFilter = GL_NEAREST,
            .flipVertically = true,
            .gammaCorrected = true,
            .path = colorFileName
        });

        remoteGBufferSize = glm::uvec2(colorTexture->width, colorTexture->height);

        remoteCamera = new PerspectiveCamera(remoteGBufferSize.x, remoteGBufferSize.y);
        remoteCamera->setFovyDegrees(90.0f);
        remoteCamera->setPosition(glm::vec3(0.0f, 3.0f, 10.0f));
        remoteCamera->updateViewMatrix();

        quadSet = std::make_unique<QuadSet>(remoteGBufferSize);
        quadMesh = std::make_unique<QuadMesh>(*quadSet, *colorTexture);

        // Load quad buffers and depth offsets
        double startTime = timeutils::getTimeMicros();
        frame.loadFromFiles(dataPath);
        loadFromFilesTime = timeutils::microsToMillis(timeutils::getTimeMicros() - startTime);

        startTime = timeutils::getTimeMicros();
        auto sizes = quadSet->unmapFromCPU(frame.quads, frame.depthOffsets);
        transferTime = quadSet->stats.timeToTransferMs;

        spdlog::info("Time to load from files: {:.3f}ms", loadFromFilesTime);
        spdlog::info("Time to transfer to GPU: {:.3f}ms", transferTime);
        spdlog::info("Loaded {} quads ({:.3f} MB), {} depth offsets ({:.3f} MB)",
                     sizes.numQuads, sizes.quadsSize / BYTES_PER_MEGABYTE,
                     sizes.numDepthOffsets, sizes.depthOffsetsSize / BYTES_PER_MEGABYTE);

        node = new Node(quadMesh.get());
        node->frustumCulled = false;
        node->setPosition(-1.0f * remoteCamera->getPosition());
        scene->addChildNode(node);

        nodeWireframe = new Node(quadMesh.get());
        nodeWireframe->frustumCulled = false;
        nodeWireframe->wireframe = true;
        nodeWireframe->visible = false;
        nodeWireframe->primativeType = GL_LINES;
        nodeWireframe->overrideMaterial = new QuadMaterial({ .baseColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) });
        nodeWireframe->setPosition(-1.0f * remoteCamera->getPosition());
        scene->addChildNode(nodeWireframe);
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
            OPENXR_CHECK(xrGetActionStateBoolean(session, &actionStateGetInfo, &clickState[i]), "Failed to get Boolean State of Click action.");

            actionStateGetInfo.action = thumbstickAction;
            actionStateGetInfo.subactionPath = handPaths[i];
            OPENXR_CHECK(xrGetActionStateVector2f(session, &actionStateGetInfo, &thumbstickState[i]), "Failed to get Vector2f State of Thumbstick action.");

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
            OPENXR_CHECK(xrApplyHapticFeedback(session, &hapticActionInfo, (XrHapticBaseHeader* )&vibration), "Failed to apply haptic feedback.");
        }
    }

    void HandleInteractions() override {
        // For each hand:
        for (int i = 0; i < 2; i++) {
            // Draw the controllers:
            handNodes[i].visible = handPoseState[i].isActive;

            if (clickState[i].isActive == XR_TRUE && clickState[i].currentState == XR_FALSE && clickState[i].changedSinceLastSync == XR_TRUE) {
                // XR_LOG("Click action triggered for hand: " << i);
                buzz[i] = 0.5f;

                nodeWireframe->visible = !nodeWireframe->visible;
            }

            if (thumbstickState[i].isActive == XR_TRUE && thumbstickState[i].changedSinceLastSync == XR_TRUE) {
                if (glm::abs(thumbstickState[i].currentState.x) > 0.2f || glm::abs(thumbstickState[i].currentState.y) > 0.2f) {
                    const glm::vec3 &forward = cameras->left.getForwardVector();
                    const glm::vec3 &right = cameras->left.getRightVector();
                    cameraPositionOffset += movementSpeed * forward * thumbstickState[i].currentState.y;
                    cameraPositionOffset += movementSpeed * right * thumbstickState[i].currentState.x;
                }
                // XR_LOG("Thumbstick action triggered for hand: " << i << " with value: " << thumbstickState[i].currentState.x << ", " << thumbstickState[i].currentState.y);
            }
        }
    }

    void OnRender(double now, double dt) override {
        quadMesh->appendQuads(*quadSet, remoteGBufferSize);
        quadMesh->createMeshFromProxies(*quadSet, remoteGBufferSize, *remoteCamera);

        graphicsAPI->drawObjects(*scene, *cameras);

        spdlog::info("Time to append proxies: {:.3f}ms", quadMesh->stats.timeToAppendQuadsMs);
        spdlog::info("Time to fill output quads: {:.3f}ms", quadMesh->stats.timeToGatherQuadsMs);
        spdlog::info("Time to create mesh: {:.3f}ms", quadMesh->stats.timeToCreateMeshMs);
        spdlog::info("Rendering time: {:.3f}ms", timeutils::secondsToMillis(dt));
    }

    void DestroyResources() override {
        delete colorTexture;
        delete node;
    }

private:
    double loadFromFilesTime = 0.0;
    double transferTime = 0.0;

    glm::uvec2 remoteGBufferSize;
    PerspectiveCamera* remoteCamera;

    std::unique_ptr<QuadSet> quadSet;

    Texture* colorTexture;
    std::unique_ptr<QuadMesh> quadMesh;
    Node* node;
    Node* nodeWireframe;

    std::unique_ptr<Model> leftControllerModel;
    std::unique_ptr<Model> rightControllerModel;

    ReferenceFrame frame;

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
};

#endif // QUADS_VIEWER_H
