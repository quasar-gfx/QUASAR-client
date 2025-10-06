#ifndef MESHWARP_CLIENT_H
#define MESHWARP_CLIENT_H

#include <OpenXRApp.h>

#include <Primitives/Mesh.h>
#include <Primitives/Cube.h>
#include <Primitives/Model.h>
#include <Materials/UnlitMaterial.h>
#include <Lights/AmbientLight.h>
#include <PostProcessing/Tonemapper.h>

#include <Cameras/PerspectiveCamera.h>
#include <Utils/FileIO.h>

#include <Receivers/MeshWarpReceiver.h>
#include <Streamers/PoseStreamer.h>

#include <shaders_common.h>

#define GEN_MESH_THREADS_PER_LOCALGROUP 16

using namespace quasar;

class MeshWarpClient final : public OpenXRApp {
private:
    std::string serverIP = "192.168.22.227";
    std::string poseURL = serverIP + ":54321";
    std::string videoURL = "0.0.0.0:12345";
    std::string depthURL = serverIP + ":65432";

    const glm::uvec2 videoSize{1920, 1080};

    uint vertexGroupSize = 1;
    uint depthFactor = 4;
    float remoteFOV = 120.0f;

public:
    MeshWarpClient() = default;
    ~MeshWarpClient() = default;

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

        // Create MeshWarpReceiver which encapsulates streaming and mesh generation
        meshWarpReceiver = std::make_unique<MeshWarpReceiver>(videoSize, depthFactor, vertexGroupSize, remoteFOV, videoURL, depthURL);

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

        // Create pose streamer bound to receiver's remote camera
        poseStreamer = std::make_unique<PoseStreamer>(&meshWarpReceiver->getRemoteCamera(), poseURL);

        // // Add a screen for the video
        // Cube* videoScreen = new Cube({
        //     .material = new UnlitMaterial({ baseColorTexture = videoTexture }),
        // });
        // Node* screen = new Node(videoScreen);
        // screen->setPosition({ 0.0f, 0.0f, -20f });
        // screen->setScale({ 1.0f, 0.5f, 005f });
        // screen->frustumCulled = false;
        // scene->addChildNode(screen);
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
        for (int i = 0; i < 2; i++) {
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
        // Update and send pose
        const glm::vec3& headPosition = (cameras->left.getPosition() + cameras->right.getPosition()) / 2.0f;
        const glm::quat& headRotation = glm::normalize(glm::slerp(
            cameras->left.getRotationQuat(),
            cameras->right.getRotationQuat(),
            0.5f
        ));
        auto& remoteCamera = meshWarpReceiver->getRemoteCamera();
        remoteCamera.setPosition(headPosition);
        remoteCamera.setRotationQuat(headRotation);
        remoteCamera.updateViewMatrix();
        poseStreamer->sendPose();

        // Receive and update mesh
        meshWarpReceiver->recvData(*poseStreamer, elapsedTimeColor, elapsedTimeDepth);
        poseStreamer->removePosesLessThan(std::min(meshWarpReceiver->poseIdColor, meshWarpReceiver->poseIdDepth));

        // Render
        renderStats = renderer->drawObjects(*scene, *cameras);
        tonemapper->drawToScreen(*renderer);
        // spdlog::info("Total Frame time: {:.3f}ms", timeutils::secondsToMillis(dt));

        if (glm::abs(elapsedTimeColor) > 1e-5f) {
            XR_LOG("E2E Latency (RGB): " << elapsedTimeColor << "ms");
        }
        if (glm::abs(elapsedTimeDepth) > 1e-5f) {
            XR_LOG("E2E Latency (D): " << elapsedTimeDepth << "ms");
        }
    }

    void DestroyResources() override {
        // Resources are automatically cleaned up by unique_ptr
    }

private:
    std::unique_ptr<Tonemapper> tonemapper;
    std::unique_ptr<PoseStreamer> poseStreamer;

    // Timing/pose ids returned by the receiver for latest frames
    pose_id_t poseIdColor = -1, poseIdDepth = -1;
    double elapsedTimeColor, elapsedTimeDepth;
    Pose currentColorFramePose, currentDepthFramePose;

    // Remote camera is owned by the receiver; access through meshWarpReceiver->getRemoteCamera()
    std::unique_ptr<MeshWarpReceiver> meshWarpReceiver;

    Node node, nodeWireframe;

    RenderStats renderStats;

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

#endif // MESHWARP_CLIENT_H
