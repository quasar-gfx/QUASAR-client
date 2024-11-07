#ifndef MODEL_VIEWER_H
#define MODEL_VIEWER_H

#include <OpenXRApp.h>

#include <Primatives/Mesh.h>
#include <Primatives/Cube.h>
#include <Primatives/Model.h>

#include <Lights/AmbientLight.h>
#include <Lights/DirectionalLight.h>
#include <Lights/PointLight.h>

class ModelViewer final : public OpenXRApp {
private:
    unsigned int surfelSize = 2;
    glm::uvec2 windowSize = glm::uvec2(1024, 1024);

    bool meshWarpEnabled = true;

public:
    ModelViewer(GraphicsAPI_Type apiType) : OpenXRApp(apiType) {}
    ~ModelViewer() = default;

private:
    void CreateResources() override {
        scene->backgroundColor = glm::vec4(0.17f, 0.17f, 0.17f, 1.0f);

        AmbientLight* ambientLight = new AmbientLight({
            .intensity = 0.01f
        });
        scene->setAmbientLight(ambientLight);

        DirectionalLight* directionalLight = new DirectionalLight({
            .color = glm::vec3(1.0f, 1.0f, 1.0f),
            .direction = glm::vec3(-0.5f, -1.0f, 0.5f),
            .intensity = 3.0f
        });
        scene->setDirectionalLight(directionalLight);

        PointLight* pointLight1 = new PointLight({
            .color = glm::vec3(0.0f, 0.0f, 1.0f),
            .position = glm::vec3(2.0f, 0.0f, 2.0f),
            .intensity = 1.0f,
            .constant = 1.0f,
            .linear = 0.09f,
            .quadratic = 0.032f
        });
        scene->addPointLight(pointLight1);

        PointLight* pointLight2 = new PointLight({
            .color = glm::vec3(0.0f, 1.0f, 0.0f),
            .position = glm::vec3(2.0f, 0.0f, -2.0f),
            .intensity = 1.0f,
            .constant = 1.0f,
            .linear = 0.09f,
            .quadratic = 0.032f
        });
        scene->addPointLight(pointLight2);

        PointLight* pointLight3 = new PointLight({
            .color = glm::vec3(1.0f, 0.0f, 0.0f),
            .position = glm::vec3(-2.0f, 0.0f, 2.0f),
            .intensity = 1.0f,
            .constant = 1.0f,
            .linear = 0.09f,
            .quadratic = 0.032f
        });
        scene->addPointLight(pointLight3);

        PointLight* pointLight4 = new PointLight({
            .color = glm::vec3(1.0f, 0.0f, 1.0f),
            .position = glm::vec3(-2.0f, 0.0f, -2.0f),
            .intensity = 1.0f,
            .constant = 1.0f,
            .linear = 0.09f,
            .quadratic = 0.032f
        });
        scene->addPointLight(pointLight4);

        // add the hand nodes
        Model* leftControllerMesh = new Model({
            .flipTextures = true,
            .IBL = 0,
            .path = "models/oculus-touch-controller-v3-left.glb"
        });
        m_handNodes[0].setEntity(leftControllerMesh);

        Model* rightControllerMesh = new Model({
            .flipTextures = true,
            .IBL = 0,
            .path = "models/oculus-touch-controller-v3-right.glb"
        });
        m_handNodes[1].setEntity(rightControllerMesh);

        // add floor
        Cube* floorMesh = new Cube({
            .material = new PBRMaterial({
                .albedoTexturePath = "textures/pbr/gold/albedo.png",
                .normalTexturePath = "textures/pbr/gold/normal.png",
                .metallicTexturePath = "textures/pbr/gold/metallic.png",
                .roughnessTexturePath = "textures/pbr/gold/roughness.png",
                .aoTexturePath = "textures/pbr/gold/ao.png"
            })
        });
        Node* floor = new Node(floorMesh);
        floor->setPosition(glm::vec3(0.0f, -m_viewHeightM, 0.0f));
        floor->setScale(glm::vec3(2.5f, 0.05f, 2.5f));
        floor->frustumCulled = false;
        scene->addChildNode(floor);

        // add helmet
        Model* helmetMesh = new Model({
            .flipTextures = true,
            .IBL = 0,
            .path = "models/DamagedHelmet.glb"
        });
        Node* helmet = new Node(helmetMesh);
        helmet->setPosition(glm::vec3(0.0f, 0.0f, -1.0f));
        helmet->setScale(glm::vec3(0.25f, 0.25f, 0.25f));
        scene->addChildNode(helmet);
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
                if (glm::abs(m_thumbstickState[i].currentState.x) > 0.2f || glm::abs(m_thumbstickState[i].currentState.y) > 0.2f)
                    cameraPositionOffset += movementSpeed * glm::vec3(m_thumbstickState[i].currentState.x, 0.0f, -m_thumbstickState[i].currentState.y);
                XR_LOG("Thumbstick action triggered for hand: " << i << " with value: " << m_thumbstickState[i].currentState.x << ", " << m_thumbstickState[i].currentState.y);
            }
        }
    }

    void OnRender() override {
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
    float movementSpeed = 0.01f;
    // The haptic output action for grabbing cubes.
    XrAction m_buzzAction;
    // The current haptic output value for each controller.
    float m_buzz[2] = {0, 0};
};


#endif // MODEL_VIEWER_H
