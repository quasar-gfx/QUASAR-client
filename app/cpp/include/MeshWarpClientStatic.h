#ifndef MESHWARP_CLIENT_STATIC_H
#define MESHWARP_CLIENT_STATIC_H

#include <OpenXRApp.h>

#include <Primatives/Mesh.h>
#include <Primatives/Model.h>

#include <Materials/UnlitMaterial.h>
#include <Buffer.h>
#include <Utils/FileIO.h>
#include <Cameras/PerspectiveCamera.h>
#include <BC4DepthVideoTexture.h>

#include <shaders_common.h>

#define THREADS_PER_LOCALGROUP 16

class MeshWarpClientStatic final : public OpenXRApp {
private:
    unsigned int surfelSize = 2;
    glm::uvec2 windowSize = glm::uvec2(1024, 1024);

    bool meshWarpEnabled = true;

public:
    MeshWarpClientStatic(GraphicsAPI_Type apiType) : OpenXRApp(apiType) {}
    ~MeshWarpClientStatic() = default;

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

        // create texture
        colorTexture = new Texture({
            .wrapS = GL_CLAMP_TO_EDGE,
            .wrapT = GL_CLAMP_TO_EDGE,
            .minFilter = GL_LINEAR_MIPMAP_LINEAR,
            .magFilter = GL_LINEAR,
            .flipVertically = true,
            .gammaCorrected = true,
            .path = "static/color.png"
        });

        // remote camera
        remoteCamera = new PerspectiveCamera(colorTexture->width, colorTexture->height);
        remoteCamera->updateViewMatrix();

        // Load BC4 depth buffer
        auto depthData = FileIO::loadBinaryFile("static/depth.bc4");

        bc4BufferData = new Buffer<BC4DepthVideoTexture::Block>(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_DRAW, windowSize.x/8*windowSize.y/8, reinterpret_cast<BC4DepthVideoTexture::Block*>(depthData.data()));

        // Setup scene & mesh
        glm::uvec2 adjustedWindowSize = windowSize / surfelSize;
        unsigned int maxVertices = adjustedWindowSize.x * adjustedWindowSize.y;
        unsigned int numTriangles = (adjustedWindowSize.x-1) * (adjustedWindowSize.y-1) * 2;
        unsigned int maxIndices = numTriangles * 3;

        mesh = new Mesh({
            .vertices = std::vector<Vertex>(maxVertices),
            .indices = std::vector<unsigned int>(maxIndices),
            .material = new UnlitMaterial({ .baseColorTexture = colorTexture }),
            .usage = GL_DYNAMIC_DRAW
        });
        node = new Node(mesh);
        node->frustumCulled = false;
        scene->addChildNode(node);

        genMeshFromBC4Shader = new ComputeShader({
            .computeCodeData = SHADER_COMMON_GENMESHFROMBC4_COMP,
            .computeCodeSize = SHADER_COMMON_GENMESHFROMBC4_COMP_len,
            .defines = {
                "#define THREADS_PER_LOCALGROUP " + std::to_string(THREADS_PER_LOCALGROUP)
            }
        });
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
        genMeshFromBC4Shader->bind();

        genMeshFromBC4Shader->setBool("unlinearizeDepth", true);

        genMeshFromBC4Shader->setVec2("screenSize", windowSize);
        genMeshFromBC4Shader->setVec2("depthMapSize", glm::vec2(colorTexture->width, colorTexture->height));
        genMeshFromBC4Shader->setInt("surfelSize", surfelSize);

        genMeshFromBC4Shader->setMat4("projection", remoteCamera->getProjectionMatrix());
        genMeshFromBC4Shader->setMat4("projectionInverse", glm::inverse(remoteCamera->getProjectionMatrix()));
        genMeshFromBC4Shader->setMat4("viewInverseDepth", glm::inverse(remoteCamera->getViewMatrix()));
        genMeshFromBC4Shader->setFloat("near", remoteCamera->near);
        genMeshFromBC4Shader->setFloat("far", remoteCamera->far);

        genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 0, mesh->vertexBuffer);
        genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 1, mesh->indexBuffer);
        genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 2, *bc4BufferData);

        genMeshFromBC4Shader->dispatch(
            (windowSize.x / surfelSize + THREADS_PER_LOCALGROUP - 1) / THREADS_PER_LOCALGROUP,
            (windowSize.y / surfelSize + THREADS_PER_LOCALGROUP - 1) / THREADS_PER_LOCALGROUP,
            1
        );

        genMeshFromBC4Shader->memoryBarrier(
            GL_SHADER_STORAGE_BARRIER_BIT |
            GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT |
            GL_ELEMENT_ARRAY_BARRIER_BIT
        );

        // Render
        renderStats = m_graphicsAPI->drawObjects(*scene.get(), *cameras.get());
    }

    void DestroyResources() override {
        delete colorTexture;
        delete bc4BufferData;
        delete mesh;
        delete node;
        delete genMeshFromBC4Shader;
        delete remoteCamera;
    }

    PerspectiveCamera* remoteCamera;

    Texture* colorTexture;
    Buffer<BC4DepthVideoTexture::Block>* bc4BufferData;
    Mesh* mesh;
    Node* node;
    ComputeShader* genMeshFromBC4Shader;

    RenderStats renderStats;

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

#endif // MESHWARP_CLIENT_STATIC_H
