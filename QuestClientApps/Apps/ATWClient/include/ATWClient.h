#ifndef ATW_CLIENT_H
#define ATW_CLIENT_H

#include <OpenXRApp.h>

#include <Primitives/Mesh.h>
#include <Primitives/Cube.h>
#include <Primitives/Model.h>

#include <Materials/UnlitMaterial.h>
#include <Lights/AmbientLight.h>

#include <PoseStreamer.h>
#include <VideoTexture.h>

#include <shaders_common.h>

using namespace quasar;

class ATWClient final : public OpenXRApp {
private:
    std::string serverIP = "192.168.4.140";
    std::string poseURL = serverIP + ":54321";
    std::string videoURL = "0.0.0.0:12345";

    std::string videoFormat = "mpegts";
    glm::uvec2 videoSize = glm::uvec2(2048, 1024);

public:
    ATWClient(GraphicsAPI_Type apiType) : OpenXRApp(apiType) {}
    ~ATWClient() = default;

private:
    void CreateResources() override {
        scene->backgroundColor = glm::vec4(0.17f, 0.17f, 0.17f, 1.0f);

        atwShader = new Shader({
            .vertexCodeData = SHADER_BUILTIN_POSTPROCESS_VERT,
            .vertexCodeSize = SHADER_BUILTIN_POSTPROCESS_VERT_len,
            .fragmentCodeData = SHADER_COMMON_ATW_FRAG,
            .fragmentCodeSize = SHADER_COMMON_ATW_FRAG_len
        });

        videoTexture = new VideoTexture({
            .width = videoSize.x,
            .height = videoSize.y,
            .internalFormat = GL_SRGB8,
            .format = GL_RGB,
            .type = GL_UNSIGNED_BYTE,
            .wrapS = GL_CLAMP_TO_EDGE,
            .wrapT = GL_CLAMP_TO_EDGE,
            .minFilter = GL_LINEAR,
            .magFilter = GL_LINEAR
        }, videoURL, videoFormat);

        poseStreamer = std::make_unique<PoseStreamer>(cameras.get(), poseURL);

        // add the hand nodes.
        Model* leftControllerMesh = new Model({
            .flipTextures = true,
            .IBL = 0,
            .path = "models/quest-touch-plus-left.glb"
        });
        m_handNodes[0].setEntity(leftControllerMesh);

        Model* rightControllerMesh = new Model({
            .flipTextures = true,
            .IBL = 0,
            .path = "models/quest-touch-plus-right.glb"
        });
        m_handNodes[1].setEntity(rightControllerMesh);

        AmbientLight* ambientLight = new AmbientLight({
            .intensity = 0.5f
        });
        scene->setAmbientLight(ambientLight);

        // add a screen for the video.
        Cube* videoScreen = new Cube({
            .material = new UnlitMaterial({ .baseColorTexture = videoTexture }),
        });
        Node* screen = new Node(videoScreen);
        screen->setPosition(glm::vec3(0.0f, 0.0f, -2.0f));
        screen->setScale(glm::vec3(1.0f, 0.5f, 0.05f));
        screen->frustumCulled = false;
        scene->addChildNode(screen);
    }

    void CreateActionSet() override {
        // An Action for clicking on the controller.
        CreateAction(m_clickAction, "click-controller", XR_ACTION_TYPE_BOOLEAN_INPUT, {"/user/hand/left", "/user/hand/right"});
        // An Action for the position of the thumbstick.
        CreateAction(m_thumbstickAction, "thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, {"/user/hand/left", "/user/hand/right"});
        // An Action for a vibration output on one or other hand.
        CreateAction(m_buzzAction, "buzz", XR_ACTION_TYPE_VIBRATION_OUTPUT, {"/user/hand/left", "/user/hand/right"});
    }

    void SuggestBindings(std::map<std::string, std::vector<XrActionSuggestedBinding>>& bindings) override {
        bindings["/interaction_profiles/khr/simple_controller"].push_back({m_clickAction, CreateXrPath("/user/hand/left/input/select/click")});
        bindings["/interaction_profiles/khr/simple_controller"].push_back({m_clickAction, CreateXrPath("/user/hand/right/input/select/click")});
        bindings["/interaction_profiles/khr/simple_controller"].push_back({m_buzzAction, CreateXrPath("/user/hand/left/output/haptic")});
        bindings["/interaction_profiles/khr/simple_controller"].push_back({m_buzzAction, CreateXrPath("/user/hand/right/output/haptic")});

        bindings["/interaction_profiles/oculus/touch_controller"].push_back({m_clickAction, CreateXrPath("/user/hand/left/input/trigger/value")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({m_clickAction, CreateXrPath("/user/hand/right/input/trigger/value")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({m_thumbstickAction, CreateXrPath("/user/hand/left/input/thumbstick")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({m_thumbstickAction, CreateXrPath("/user/hand/right/input/thumbstick")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({m_buzzAction, CreateXrPath("/user/hand/left/output/haptic")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({m_buzzAction, CreateXrPath("/user/hand/right/output/haptic")});
    }

    void PollActions(XrTime predictedTime) override {
        XrActionStateGetInfo actionStateGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};

        for (int i = 0; i < 2; i++) {
            actionStateGetInfo.action = m_clickAction;
            actionStateGetInfo.subactionPath = m_handPaths[i];
            OPENXR_CHECK(xrGetActionStateBoolean(m_session, &actionStateGetInfo, &m_clickState[i]),
                                                 "Failed to get Boolean State of Click action.");

            actionStateGetInfo.action = m_thumbstickAction;
            actionStateGetInfo.subactionPath = m_handPaths[i];
            OPENXR_CHECK(xrGetActionStateVector2f(m_session, &actionStateGetInfo, &m_thumbstickState[i]),
                                                 "Failed to get Vector2f State of Thumbstick action.");

            m_buzz[i] *= 0.5f;
            if (m_buzz[i] < 0.01f)
                m_buzz[i] = 0.0f;
            XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
            vibration.amplitude = m_buzz[i];
            vibration.duration = XR_MIN_HAPTIC_DURATION;
            vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

            XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
            hapticActionInfo.action = m_buzzAction;
            hapticActionInfo.subactionPath = m_handPaths[i];
            OPENXR_CHECK(xrApplyHapticFeedback(m_session, &hapticActionInfo, (XrHapticBaseHeader* )&vibration),
                                               "Failed to apply haptic feedback.");
        }
    }

    void HandleInteractions() override {
        // For each hand:
        for (int i = 0; i < 2; i++) {
            // Draw the controllers:
            m_handNodes[i].visible = m_handPoseState[i].isActive;

            if (m_clickState[i].isActive == XR_TRUE && m_clickState[i].currentState == XR_FALSE && m_clickState[i].changedSinceLastSync == XR_TRUE) {
                // XR_LOG("Click action triggered for hand: " << i);
                m_buzz[i] = 0.5f;
                atwEnabled = !atwEnabled;
            }

            if (m_thumbstickState[i].isActive == XR_TRUE && m_thumbstickState[i].changedSinceLastSync == XR_TRUE) {
                if (glm::abs(m_thumbstickState[i].currentState.x) > 0.2f || glm::abs(m_thumbstickState[i].currentState.y) > 0.2f) {
                    const glm::vec3 &forward = cameras.get()->left.getForwardVector();
                    const glm::vec3 &right = cameras.get()->left.getRightVector();
                    cameraPositionOffset += movementSpeed * forward * m_thumbstickState[i].currentState.y;
                    cameraPositionOffset += movementSpeed * right * m_thumbstickState[i].currentState.x;
                }
                // XR_LOG("Thumbstick action triggered for hand: " << i << " with value: " << m_thumbstickState[i].currentState.x << ", " << m_thumbstickState[i].currentState.y);
            }
        }
    }

    void OnRender(double now, double dt) override {
        // send pose
        poseStreamer->sendPose();

        // render video to VideoTexture
        videoTexture->bind();
        poseID = videoTexture->draw();

        // set uniforms for both eyes
        atwShader->bind();

        atwShader->setBool("atwEnabled", atwEnabled);
        atwShader->setBool("toneMap", false);

        atwShader->setMat4("projectionInverseLeft", glm::inverse(cameras->left.getProjectionMatrix()));
        atwShader->setMat4("projectionInverseRight", glm::inverse(cameras->right.getProjectionMatrix()));

        atwShader->setMat4("viewInverseLeft", glm::inverse(cameras->left.getViewMatrix()));
        atwShader->setMat4("viewInverseRight", glm::inverse(cameras->right.getViewMatrix()));

        double elapsedTime;
        if (poseID != prevPoseID && poseStreamer->getPose(poseID, &currentFramePose, &elapsedTime)) {
            atwShader->setMat4("remoteProjectionLeft", currentFramePose.stereo.projL);
            atwShader->setMat4("remoteProjectionRight", currentFramePose.stereo.projR);

            atwShader->setMat4("remoteViewLeft", currentFramePose.stereo.viewL);
            atwShader->setMat4("remoteViewRight", currentFramePose.stereo.viewR);

            poseStreamer->removePosesLessThan(poseID);
        }
        atwShader->setTexture("videoTexture", *videoTexture, 0);

        // draw both eyes in a single pass
        m_graphicsAPI->drawToScreen(*atwShader);

        prevPoseID = poseID;

        // draw objects (uncomment to debug)
        // m_graphicsAPI->drawObjects(*scene.get(), *cameras.get(), GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        if (glm::abs(elapsedTime) > 1e-5f) {
            XR_LOG("E2E Latency: " << elapsedTime << "ms");
        }

        spdlog::info("Rendering time: {:.3f}ms", timeutils::secondsToMillis(dt));
    }

    void DestroyResources() override {
        delete videoTexture;
        delete atwShader;
    }

    // Shader for the ATW effect.
    Shader* atwShader;
    bool atwEnabled = true;

    VideoTexture* videoTexture;

    // Pose streaming.
    pose_id_t poseID = -1;
    pose_id_t prevPoseID = -1;
    std::unique_ptr<PoseStreamer> poseStreamer;
    Pose currentFramePose;

    // Actions.
    XrAction m_clickAction;
    // The realtime states of these actions.
    XrActionStateBoolean m_clickState[2] = {{XR_TYPE_ACTION_STATE_BOOLEAN}, {XR_TYPE_ACTION_STATE_BOOLEAN}};
    // The thumbstick input action.
    XrAction m_thumbstickAction;
    // The current thumbstick state for each controller.
    XrActionStateVector2f m_thumbstickState[2] = {{XR_TYPE_ACTION_STATE_VECTOR2F}, {XR_TYPE_ACTION_STATE_VECTOR2F}};
    float movementSpeed = 0.02f;
    // The haptic output action for grabbing cubes.
    XrAction m_buzzAction;
    // The current haptic output value for each controller.
    float m_buzz[2] = {0, 0};
};

#endif // ATW_CLIENT_H
