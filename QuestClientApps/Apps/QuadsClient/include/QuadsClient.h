#ifndef QUADS_CLIENT_H
#define QUADS_CLIENT_H

#include <OpenXRApp.h>

#include <Path.h>
#include <Primitives/Mesh.h>
#include <Primitives/Cube.h>
#include <Primitives/Model.h>
#include <Materials/UnlitMaterial.h>
#include <Lights/AmbientLight.h>

#include <Receivers/QuadsReceiver.h>
#include <Streamers/PoseStreamer.h>

using namespace quasar;

class QuadsClient final : public OpenXRApp {
private:
    std::string serverIP = "192.168.4.140";
    std::string poseURL = serverIP + ":54321";
    std::string videoURL = "0.0.0.0:12345";
    std::string quadsURL = serverIP + ":65432";

    const glm::uvec2 remoteGBufferSize = glm::uvec2(1920, 1080);

    float remoteFOV = 90.0f;

public:
    QuadsClient(GraphicsAPI_Type apiType)
        : OpenXRApp(apiType)
        , remoteCamera(remoteGBufferSize)
    {}
    ~QuadsClient() = default;

private:
    void CreateResources() override {
        scene->backgroundColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        AmbientLight* ambientLight = new AmbientLight({
            .intensity = 1.0f
        });
        scene->setAmbientLight(ambientLight);

        // Add the hand nodes.
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
        quadsReceiver = std::make_unique<QuadsReceiver>(*quadSet, videoURL, quadsURL);

        // Create pose streamer
        remoteCamera.setFovyDegrees(remoteFOV);
        poseStreamer = std::make_unique<PoseStreamer>(&remoteCamera, poseURL);

        // Create nodes
        refNode.setEntity(&quadsReceiver->getReferenceMesh());
        refNode.frustumCulled = false;
        scene->addChildNode(&refNode);

        refNodeWireframe.setEntity(&quadsReceiver->getReferenceMesh());
        refNodeWireframe.frustumCulled = false;
        refNodeWireframe.wireframe = true;
        refNodeWireframe.visible = false;
        refNodeWireframe.primitiveType = GL_LINES;
        refNodeWireframe.overrideMaterial = new QuadMaterial({ .baseColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) });
        scene->addChildNode(&refNodeWireframe);

        resNode.setEntity(&quadsReceiver->getResidualMesh());
        resNode.frustumCulled = false;
        scene->addChildNode(&resNode);

        resNodeWireframe.setEntity(&quadsReceiver->getResidualMesh());
        resNodeWireframe.frustumCulled = false;
        resNodeWireframe.wireframe = true;
        resNodeWireframe.visible = false;
        resNodeWireframe.primitiveType = GL_LINES;
        resNodeWireframe.overrideMaterial = new QuadMaterial({ .baseColor = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) });
        scene->addChildNode(&resNodeWireframe);

        // // Add a screen for the video
        // Cube* videoScreen = new Cube({
        //     .material = new UnlitMaterial({ .baseColorTexture = &quadsReceiver->atlasVideoTexture }),
        // });
        // Node* screen = new Node(videoScreen);
        // screen->setPosition({ 0.0f, 0.0f, -2.0f });
        // screen->setScale({ 1.5f, 0.5f, 0.05f });
        // screen->frustumCulled = false;
        // scene->addChildNode(screen);
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

                showWireframe = !showWireframe;
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
        // Update and send pose
        const glm::vec3& headPosition = (cameras->left.getPosition() + cameras->right.getPosition()) / 2.0f;
        const glm::quat& headRotation = glm::normalize(glm::slerp(
            cameras->left.getRotationQuat(),
            cameras->right.getRotationQuat(),
            0.5f
        ));
        remoteCamera.setPosition(headPosition);
        remoteCamera.setRotationQuat(headRotation);
        remoteCamera.updateViewMatrix();
        poseStreamer->sendPose();

        FrameType frameType = quadsReceiver->recvData();
        if (frameType != FrameType::NONE) {
            resNode.visible = frameType == FrameType::RESIDUAL;

            spdlog::info("Time to load: {:.3f}ms", quadsReceiver->stats.timeToLoadMs);
            spdlog::info("Time to decompress: {:.3f}ms", quadsReceiver->stats.timeToDecompressMs);
            spdlog::info("Time to transfer to GPU: {:.3f}ms", quadsReceiver->stats.timeToTransferMs);
            spdlog::info("Time to create mesh: {:.3f}ms", quadsReceiver->stats.timeToCreateMeshMs);
            spdlog::info("Loaded {} quads ({:.3f} MB), {} depth offsets ({:.3f} MB)",
                        quadsReceiver->stats.sizes.numQuads, quadsReceiver->stats.sizes.quadsSize / BYTES_PER_MEGABYTE,
                        quadsReceiver->stats.sizes.numDepthOffsets, quadsReceiver->stats.sizes.depthOffsetsSize / BYTES_PER_MEGABYTE);
        }

        refNodeWireframe.visible = showWireframe;
        resNodeWireframe.visible = resNode.visible && showWireframe;

        graphicsAPI->drawObjects(*scene, *cameras);
        // spdlog::info("Total Frame time: {:.3f}ms", timeutils::secondsToMillis(dt));
    }

    void DestroyResources() override {}

private:
    std::unique_ptr<QuadSet> quadSet;
    std::unique_ptr<QuadsReceiver> quadsReceiver;

    Node refNode, refNodeWireframe;
    Node resNode, resNodeWireframe;
    bool showWireframe = false;

    PerspectiveCamera remoteCamera;

    // Pose streaming.
    pose_id_t prevPoseID = -1;
    std::unique_ptr<PoseStreamer> poseStreamer;

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

#endif // QUADS_CLIENT_H
