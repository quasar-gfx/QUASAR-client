#ifndef ATW_CLIENT_H
#define ATW_CLIENT_H

#include <OpenXRApp.h>

#include <Primitives/Mesh.h>
#include <Primitives/Cube.h>
#include <Primitives/Model.h>
#include <Materials/UnlitMaterial.h>
#include <Lights/AmbientLight.h>

#include <Receivers/VideoTexture.h>
#include <Streamers/PoseStreamer.h>

#include <shaders_common.h>

using namespace quasar;

class ATWClient final : public OpenXRApp {
private:
    std::string serverIP = "192.168.4.140";
    std::string poseURL = serverIP + ":54321";
    std::string videoURL = "0.0.0.0:12345";

    glm::uvec2 videoSize = glm::uvec2(2048, 1024);

public:
    ATWClient(GraphicsAPI_Type apiType) : OpenXRApp(apiType) {}
    ~ATWClient() = default;

private:
    void CreateResources() override {
        scene->setAmbientLight(new AmbientLight({ .intensity = 1.0f }));

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

        // Create video texture
        videoTexture = new VideoTexture({
            .width = videoSize.x,
            .height = videoSize.y,
            .internalFormat = GL_RGB8,
            .format = GL_RGB,
            .type = GL_UNSIGNED_BYTE,
            .wrapS = GL_CLAMP_TO_EDGE,
            .wrapT = GL_CLAMP_TO_EDGE,
            .minFilter = GL_LINEAR,
            .magFilter = GL_LINEAR
        }, videoURL);

        // Create pose streamer
        poseStreamer = std::make_unique<PoseStreamer>(cameras.get(), poseURL);

        // // Add a screen for the video
        // Cube* videoScreen = new Cube({
        //     .material = new UnlitMaterial({ .baseColorTexture = videoTexture }),
        // });
        // Node* screen = new Node(videoScreen);
        // screen->setPosition({ 0.0f, 0.0f, -2.0f });
        // screen->setScale({ 1.0f, 0.5f, 0.05f });
        // screen->frustumCulled = false;
        // scene->addChildNode(screen);

        atwShader = std::make_unique<Shader>(ShaderDataCreateParams{
            .vertexCodeData = SHADER_BUILTIN_POSTPROCESS_VERT,
            .vertexCodeSize = SHADER_BUILTIN_POSTPROCESS_VERT_len,
            .fragmentCodeData = SHADER_COMMON_ATW_FRAG,
            .fragmentCodeSize = SHADER_COMMON_ATW_FRAG_len
        });
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
            OPENXR_CHECK(xrApplyHapticFeedback(session, &hapticActionInfo, (XrHapticBaseHeader* )&vibration),
                                               "Failed to apply haptic feedback.");
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
                atwEnabled = !atwEnabled;
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
        // Send pose
        poseStreamer->sendPose();

        // Render video to VideoTexture
        videoTexture->bind();
        pose_id_t currPoseID = videoTexture->draw();

        // Set uniforms for both eyes
        atwShader->bind();

        atwShader->setBool("atwEnabled", atwEnabled);

        atwShader->setMat4("projectionInverseLeft", glm::inverse(cameras->left.getProjectionMatrix()));
        atwShader->setMat4("projectionInverseRight", glm::inverse(cameras->right.getProjectionMatrix()));

        atwShader->setMat4("viewInverseLeft", glm::inverse(cameras->left.getViewMatrix()));
        atwShader->setMat4("viewInverseRight", glm::inverse(cameras->right.getViewMatrix()));

        if (currPoseID != prevPoseID && poseStreamer->getPose(currPoseID, &currentFramePose, &elapsedTime)) {
            atwShader->setMat4("remoteProjectionLeft", currentFramePose.stereo.projL);
            atwShader->setMat4("remoteProjectionRight", currentFramePose.stereo.projR);

            atwShader->setMat4("remoteViewLeft", currentFramePose.stereo.viewL);
            atwShader->setMat4("remoteViewRight", currentFramePose.stereo.viewR);

            poseStreamer->removePosesLessThan(currPoseID);
        }
        atwShader->setTexture("videoTexture", *videoTexture, 0);

        // Draw both eyes in a single pass
        graphicsAPI->drawToScreen(*atwShader);

        // Draw objects (uncomment to debug)
        graphicsAPI->drawObjects(*scene, *cameras, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        prevPoseID = currPoseID;

        if (glm::abs(elapsedTime) > 1e-5f) {
            XR_LOG("E2E Latency: " << elapsedTime << "ms");
        }

        // spdlog::info("Total Frame time: {:.3f}ms", timeutils::secondsToMillis(dt));
    }

    void DestroyResources() override {
        delete videoTexture;
    }

    // Shader for the ATW effect.
    std::unique_ptr<Shader> atwShader;
    bool atwEnabled = true;

    VideoTexture* videoTexture;

    // Pose streaming.
    pose_id_t prevPoseID = -1;
    std::unique_ptr<PoseStreamer> poseStreamer;
    Pose currentFramePose;

    double elapsedTime = 0.0f;

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

#endif // ATW_CLIENT_H
