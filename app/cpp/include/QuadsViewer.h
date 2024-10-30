#ifndef QUADS_VIEWER_H
#define QUADS_VIEWER_H

#include <OpenXRApp.h>

#include <Primatives/Mesh.h>
#include <Primatives/Cube.h>
#include <Primatives/Model.h>

#include <Materials/UnlitMaterial.h>
#include <Lights/AmbientLight.h>

#include <PoseStreamer.h>
#include <VideoTexture.h>
#include <QuadMaterial.h>

class QuadsViewer final : public OpenXRApp {
private:
    std::string verticesFileName = "quads/vertices.bin";
    std::string indicesFileName = "quads/indices.bin";
    std::string colorFileName = "quads/color.png";

public:
    QuadsViewer(GraphicsAPI_Type apiType) : OpenXRApp(apiType) {}
    ~QuadsViewer() = default;

private:
    void CreateResources() override {
        scene->backgroundColor = glm::vec4(0.17f, 0.17f, 0.17f, 1.0f);

        AmbientLight* ambientLight = new AmbientLight({
            .intensity = 1.0f
        });
        scene->setAmbientLight(ambientLight);

        // add the hand nodes.
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

        colorTexture = new Texture({
            .wrapS = GL_REPEAT,
            .wrapT = GL_REPEAT,
            .minFilter = GL_NEAREST,
            .magFilter = GL_NEAREST,
            .flipVertically = true,
            .path = colorFileName
        });

        auto vertexData = FileIO::loadBinaryFile(verticesFileName);
        auto indexData = FileIO::loadBinaryFile(indicesFileName);

        std::vector<Vertex> vertices(vertexData.size() / sizeof(Vertex));
        std::memcpy(vertices.data(), vertexData.data(), vertexData.size());

        std::vector<unsigned int> indices(indexData.size() / sizeof(unsigned int));
        std::memcpy(indices.data(), indexData.data(), indexData.size());

        mesh = new Mesh({
            .vertices = vertices,
            .indices = indices,
            .material = new QuadMaterial({ .baseColorTexture = colorTexture }),
        });
        node = new Node(mesh);
        node->frustumCulled = false;
        node->setPosition(glm::vec3(0.0f, -3.0f, -10.0f));
        scene->addChildNode(node);

        // nodeWireframe = new Node(mesh);
        // nodeWireframe->frustumCulled = false;
        // nodeWireframe->wireframe = true;
        // nodeWireframe->visible = false;
        // nodeWireframe->overrideMaterial = new QuadMaterial({ .baseColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) });
        // nodeWireframe->setPosition(glm::vec3(0.0f, -3.0f, -10.0f));
        // scene->addChildNode(nodeWireframe);
    }

    void CreateActionSet() override {
        // An Action for clicking on the controller.
        CreateAction(m_clickAction, "click-controller", XR_ACTION_TYPE_BOOLEAN_INPUT, {"/user/hand/left", "/user/hand/right"});
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
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({m_buzzAction, CreateXrPath("/user/hand/left/output/haptic")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({m_buzzAction, CreateXrPath("/user/hand/right/output/haptic")});
    }

    void PollActions(XrTime predictedTime) override {
        XrActionStateGetInfo actionStateGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};

        for (int i = 0; i < 2; i++) {
            actionStateGetInfo.action = m_clickAction;
            actionStateGetInfo.subactionPath = m_handPaths[i];
            OPENXR_CHECK(xrGetActionStateBoolean(m_session, &actionStateGetInfo, &m_clickState[i]), "Failed to get Boolean State of Click action.");
        }

        for (int i = 0; i < 2; i++) {
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
            OPENXR_CHECK(xrApplyHapticFeedback(m_session, &hapticActionInfo, (XrHapticBaseHeader* )&vibration), "Failed to apply haptic feedback.");
        }
    }

    void HandleInteractions() override {
        // For each hand:
        for (int i = 0; i < 2; i++) {
            // Draw the controllers:
            m_handNodes[i].visible = m_handPoseState[i].isActive;

            if (m_clickState[i].isActive == XR_TRUE && m_clickState[i].currentState == XR_FALSE && m_clickState[i].changedSinceLastSync == XR_TRUE) {
                XR_LOG("Click action triggered for hand: " << i);
                m_buzz[i] = 0.5f;
                // nodeWireframe->visible = !nodeWireframe->visible;
            }
        }
    }

    void OnRender() override {
        m_graphicsAPI->drawObjects(*scene.get(), *cameras.get());
    }

    void DestroyResources() override {
    }

    Mesh* mesh;
    Texture* colorTexture;
    Node* node;
    // Node* nodeWireframe;

    // Actions.
    XrAction m_clickAction;
    // The realtime states of these actions.
    XrActionStateBoolean m_clickState[2] = {{XR_TYPE_ACTION_STATE_BOOLEAN}, {XR_TYPE_ACTION_STATE_BOOLEAN}};
    // The haptic output action for grabbing cubes.
    XrAction m_buzzAction;
    // The current haptic output value for each controller.
    float m_buzz[2] = {0, 0};
};

#endif // QUADS_VIEWER_H
