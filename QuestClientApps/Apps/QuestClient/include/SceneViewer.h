#ifndef SCENE_VIEWER_H
#define SCENE_VIEWER_H

#include <OpenXRApp.h>

#include <Primitives/Mesh.h>
#include <Primitives/Cube.h>
#include <Primitives/Model.h>

#include <Lights/AmbientLight.h>
#include <Lights/DirectionalLight.h>
#include <Lights/PointLight.h>

class SceneViewer final : public OpenXRApp {
private:
    unsigned int surfelSize = 2;
    glm::uvec2 windowSize = glm::uvec2(1024, 1024);

    bool meshWarpEnabled = true;

public:
    SceneViewer(GraphicsAPI_Type apiType) : OpenXRApp(apiType) {}
    ~SceneViewer() = default;

private:
    void CreateResources() override {
        scene->backgroundColor = glm::vec4(0.17f, 0.17f, 0.17f, 1.0f);

        // add lights
        AmbientLight* ambientLight = new AmbientLight({
            .intensity = 0.1f
        });
        scene->setAmbientLight(ambientLight);

        DirectionalLightCreateParams directionalLightParams{
            .color = glm::vec3(1.0f, 1.0f, 1.0f),
            .direction = glm::vec3(1.0f, 0.3f, -6.0f),
            .distance = 1.0f,
            .intensity = 1.25f,
            .orthoBoxSize = 50.0f
        };
        DirectionalLight* directionalLight = new DirectionalLight(directionalLightParams);
        scene->setDirectionalLight(directionalLight);

        PointLightCreateParams pointLightParams{
            .color = glm::vec3(0.95f, 0.95f, 1.0f),
            .position = glm::vec3(5.0f, 5.0f, 5.0f),
            .intensity = 2.0f,
            .constant = 1.0f,
            .linear = 0.07f,
            .quadratic = 0.017f
        };
        PointLight* pointLight = new PointLight(pointLightParams);
        scene->addPointLight(pointLight);

        pointLightParams.position = glm::vec3(-5.0f, 5.0f, 5.0f);
        pointLight = new PointLight(pointLightParams);
        scene->addPointLight(pointLight);

        pointLightParams.position = glm::vec3(5.0f, 5.0f, -5.0f);
        pointLight = new PointLight(pointLightParams);
        scene->addPointLight(pointLight);

        pointLightParams.position = glm::vec3(-5.0f, 5.0f, -5.0f);
        pointLight = new PointLight(pointLightParams);
        scene->addPointLight(pointLight);

        // add the hand nodes
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

        Model* robotLab = new Model({
            .IBL = 0,
            .flipTextures = true,
            .gammaCorrected = true,
            .path = "models/RobotLab.glb"
        });
        scene->addChildNode(new Node(robotLab));
        cameraPositionOffset += glm::vec3(0.0f, 3.0f, 10.0f);

        // // animations
        // Node* arm = robotLab->findNodeByName("prop_robotArm_body");
        // if (arm) {
        //     Animation* animation = new Animation();
        //     animation->addRotationKey(glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 360.0, 0.0), 60.0, false, true);
        //     arm->animation = animation;
        // }

        // Node* vehicle = robotLab->findNodeByName("vehicle_rcFlyer_clean");
        // if (vehicle) {
        //     Animation* animation = new Animation();
        //     animation->addPositionKey(glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 5.0, 0.0), 5.0, true, true);
        //     vehicle->animation = animation;
        // }
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
            if (m_buzz[i] < 0.01f) {
                m_buzz[i] = 0.0f;
            }
            XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
            vibration.amplitude = m_buzz[i];
            vibration.duration = XR_MIN_HAPTIC_DURATION;
            vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

            XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
            hapticActionInfo.action = m_buzzAction;
            hapticActionInfo.subactionPath = m_handPaths[i];
            OPENXR_CHECK(xrApplyHapticFeedback(m_session, &hapticActionInfo, (XrHapticBaseHeader*)&vibration),
                        "Failed to apply haptic feedback.");
        }
    }

    void HandleInteractions() override {
        // For each hand:
        for (int i = 0; i < 2; i++) {
            // Draw the controllers:
            m_handNodes[i].visible = m_handPoseState[i].isActive;

            if (m_clickState[i].isActive == XR_TRUE &&
                m_clickState[i].currentState == XR_FALSE &&
                m_clickState[i].changedSinceLastSync == XR_TRUE) {
                XR_LOG("Click action triggered for hand: " << i);
                m_buzz[i] = 0.5f;
                meshWarpEnabled = !meshWarpEnabled;
            }

            if (m_thumbstickState[i].isActive == XR_TRUE && m_thumbstickState[i].changedSinceLastSync == XR_TRUE) {
                if (glm::abs(m_thumbstickState[i].currentState.x) > 0.2f || glm::abs(m_thumbstickState[i].currentState.y) > 0.2f) {
                    const glm::vec3 &forward = cameras.get()->left.getForwardVector();
                    const glm::vec3 &right = cameras.get()->left.getRightVector();
                    cameraPositionOffset += movementSpeed * forward * m_thumbstickState[i].currentState.y;
                    cameraPositionOffset += movementSpeed * right * m_thumbstickState[i].currentState.x;
                }
                XR_LOG("Thumbstick action triggered for hand: " << i << " with value: " << m_thumbstickState[i].currentState.x << ", " << m_thumbstickState[i].currentState.y);
            }
        }
    }

    void OnRender(double now, double dt) override {
        scene->updateAnimations(dt);
        m_graphicsAPI->drawObjects(*scene.get(), *cameras.get());
    }

    void DestroyResources() override {
    }

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


#endif // SCENE_VIEWER_H
