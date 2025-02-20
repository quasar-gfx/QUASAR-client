#ifndef MESHWARP_CLIENT_STATIC_H
#define MESHWARP_CLIENT_STATIC_H

#include <OpenXRApp.h>

#include <Primitives/Mesh.h>
#include <Primitives/Model.h>
#include <Materials/UnlitMaterial.h>

#include <Buffer.h>
#include <Cameras/PerspectiveCamera.h>
#include <Utils/FileIO.h>

#include <BC4DepthVideoTexture.h>

#include <shaders_common.h>

#define GEN_MESH_THREADS_PER_LOCALGROUP 16

class MeshWarpViewer final : public OpenXRApp {
private:
    unsigned int surfelSize = 2;
    glm::uvec2 windowSize = glm::uvec2(1920, 1080);

    bool meshWarpEnabled = true;

public:
    MeshWarpViewer(GraphicsAPI_Type apiType) : OpenXRApp(apiType) {}
    ~MeshWarpViewer() = default;

private:
    void CreateResources() override {
        scene->backgroundColor = glm::vec4(0.17f, 0.17f, 0.17f, 1.0f);

        AmbientLight* ambientLight = new AmbientLight({
            .intensity = 0.5f
        });
        scene->setAmbientLight(ambientLight);

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

        // create texture
        colorTexture = new Texture({
            .wrapS = GL_CLAMP_TO_EDGE,
            .wrapT = GL_CLAMP_TO_EDGE,
            .minFilter = GL_LINEAR,
            .magFilter = GL_LINEAR,
            .flipVertically = true,
            .gammaCorrected = true,
            .path = "meshwarp/color_1920x1080.png"
        });

        // remote camera
        remoteCamera = new PerspectiveCamera(colorTexture->width, colorTexture->height);
        remoteCamera->updateViewMatrix();
        remoteCamera->setFovyDegrees(120.0f);
        // Load BC4 depth buffer
        auto depthData = FileIO::loadBinaryFile("meshwarp/depth_1920x1080.bc4");

        bc4BufferData = new Buffer(
            GL_SHADER_STORAGE_BUFFER,
            windowSize.x/8*windowSize.y/8,
            sizeof(BC4DepthVideoTexture::Block),
            reinterpret_cast<BC4DepthVideoTexture::Block*>(depthData.data()),
            GL_DYNAMIC_DRAW
        );

        // Setup scene & mesh
        glm::uvec2 adjustedWindowSize = windowSize / surfelSize;
        unsigned int maxVertices = adjustedWindowSize.x * adjustedWindowSize.y;
        unsigned int numTriangles = (adjustedWindowSize.x-1) * (adjustedWindowSize.y-1) * 2;
        unsigned int maxIndices = numTriangles * 3;

        mesh = new Mesh({
            .maxVertices = maxVertices,
            .maxIndices = maxIndices,
            .material = new UnlitMaterial({ .baseColorTexture = colorTexture }),
            .usage = GL_DYNAMIC_DRAW
        });
        node = new Node(mesh);
        node->frustumCulled = false;
        scene->addChildNode(node);

        nodeWireframe = new Node(mesh);
        nodeWireframe->frustumCulled = false;
        nodeWireframe->wireframe = true;
        nodeWireframe->visible = false;
        nodeWireframe->primativeType = GL_LINES;
        nodeWireframe->overrideMaterial = new UnlitMaterial({ .baseColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) });
        scene->addChildNode(nodeWireframe);

        genMeshFromBC4Shader = new ComputeShader({
            .computeCodeData = SHADER_COMMON_MESHFROMBC4_COMP,
            .computeCodeSize = SHADER_COMMON_MESHFROMBC4_COMP_len,
            .defines = {
                "#define THREADS_PER_LOCALGROUP " + std::to_string(GEN_MESH_THREADS_PER_LOCALGROUP)
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
                // XR_LOG("Click action triggered for hand: " << i);
                m_buzz[i] = 0.5f;

                meshWarpEnabled = !meshWarpEnabled;
                nodeWireframe->visible = !nodeWireframe->visible;
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
        double startTime = timeutils::getTimeMicros();
        genMeshFromBC4Shader->bind();

        genMeshFromBC4Shader->setBool("unlinearizeDepth", true);

        genMeshFromBC4Shader->setVec2("screenSize", windowSize);
        genMeshFromBC4Shader->setVec2("depthMapSize", glm::vec2(colorTexture->width, colorTexture->height));
        genMeshFromBC4Shader->setInt("surfelSize", surfelSize);

        genMeshFromBC4Shader->setMat4("projection", remoteCamera->getProjectionMatrix());
        genMeshFromBC4Shader->setMat4("projectionInverse", glm::inverse(remoteCamera->getProjectionMatrix()));
        genMeshFromBC4Shader->setMat4("viewInverseDepth", glm::inverse(remoteCamera->getViewMatrix()));
        genMeshFromBC4Shader->setFloat("near", remoteCamera->getNear());
        genMeshFromBC4Shader->setFloat("far", remoteCamera->getFar());

        genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 0, mesh->vertexBuffer);
        genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 1, mesh->indexBuffer);
        genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 2, *bc4BufferData);

        genMeshFromBC4Shader->dispatch(
            (windowSize.x / surfelSize + GEN_MESH_THREADS_PER_LOCALGROUP - 1) / GEN_MESH_THREADS_PER_LOCALGROUP,
            (windowSize.y / surfelSize + GEN_MESH_THREADS_PER_LOCALGROUP - 1) / GEN_MESH_THREADS_PER_LOCALGROUP,
            1
        );

        genMeshFromBC4Shader->memoryBarrier(
            GL_SHADER_STORAGE_BARRIER_BIT |
            GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT |
            GL_ELEMENT_ARRAY_BARRIER_BIT
        );
        double endTime = timeutils::getTimeMicros();

        // Render
        m_graphicsAPI->drawObjects(*scene.get(), *cameras.get());

        spdlog::info("Mesh generation time: {:.3f}ms", timeutils::microsToMillis(endTime - startTime));
        spdlog::info("Rendering time: {:.3f}ms", timeutils::secondsToMillis(dt));
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

    Buffer* bc4BufferData;

    Texture* colorTexture;
    Mesh* mesh;
    Node* node;
    Node* nodeWireframe;

    ComputeShader* genMeshFromBC4Shader;

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

#endif // MESHWARP_CLIENT_STATIC_H
