#ifndef DP_VIEWER_H
#define DP_VIEWER_H

#include <OpenXRApp.h>

#include <Primitives/Mesh.h>
#include <Primitives/Model.h>
#include <Primitives/Cube.h>

#include <Lights/AmbientLight.h>

#include <Quads/QuadMaterial.h>
#include <Quads/QuadsBuffers.h>
#include <Quads/DepthOffsets.h>
#include <Quads/MeshFromQuads.h>

class DPViewer final : public OpenXRApp {
private:
    std::string sceneName = "robot_lab";
    std::string dataPath = "dpwarp/" + sceneName + "/";

    glm::uvec2 windowSize = glm::uvec2(3360.0f, 1760.0f);
    glm::uvec2 halfWindowSize = windowSize / 2u;

    unsigned int maxLayers = 4;
    unsigned int maxViews;

    const std::vector<glm::vec4> colors = {
        glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), // primary view color is yellow
        glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
        glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
        glm::vec4(1.0f, 0.5f, 0.5f, 1.0f),
        glm::vec4(0.0f, 0.5f, 0.5f, 1.0f),
        glm::vec4(0.5f, 0.0f, 0.0f, 1.0f),
        glm::vec4(0.0f, 1.0f, 1.0f, 1.0f),
        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
        glm::vec4(0.0f, 0.5f, 0.0f, 1.0f),
    };

public:
    DPViewer(GraphicsAPI_Type apiType)
            : OpenXRApp(apiType)
            , remoteCamera(windowSize.x, windowSize.y)
            , remoteCameraWideFov(windowSize.x, windowSize.y)
            , maxViews(maxLayers + 1) {
        remoteCamera.setFovyDegrees(90.0f);
        remoteCameraWideFov.setFovyDegrees(120.0f);

        remoteCamera.setPosition(glm::vec3(0.0f, 3.0f, 10.0f));
        remoteCamera.updateViewMatrix();
        remoteCameraWideFov.setViewMatrix(remoteCamera.getViewMatrix());
    }
    ~DPViewer() = default;

private:
    void CreateResources() override {
        scene->backgroundColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        AmbientLight* ambientLight = new AmbientLight({
            .intensity = 0.5f
        });
        scene->setAmbientLight(ambientLight);

        // Add controller models
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

        // Create shared instances - only one of each!
        meshFromQuads = new MeshFromQuads(halfWindowSize);

        unsigned int maxProxies = halfWindowSize.x * halfWindowSize.y * NUM_SUB_QUADS;
        quadBuffers = new QuadBuffers(maxProxies);

        const glm::uvec2 depthBufferSize = 2u * halfWindowSize;
        depthOffsets = new DepthOffsets(depthBufferSize);

        // Pre-allocate vectors
        meshes.reserve(maxViews);
        nodes.reserve(maxViews);
        colorTextures.reserve(maxViews);

        // Load all views
        for (int view = 0; view < maxViews; view++) {
            // Load color texture
            std::string colorFileName = dataPath + "color" + std::to_string(view) + ".png";
            Texture* colorTexture = new Texture({
                .wrapS = GL_REPEAT,
                .wrapT = GL_REPEAT,
                .minFilter = GL_NEAREST,
                .magFilter = GL_NEAREST,
                .flipVertically = true,
                .path = colorFileName
            });
            colorTextures.push_back(colorTexture);

            // Get buffer size based on whether this is the last layer
            glm::uvec2 gBufferSize = glm::uvec2(colorTexture->width, colorTexture->height) / 2u;
            if (view == maxViews - 1) {
                gBufferSize /= 2u;
            }

            unsigned int numBytes;

            // Load proxy data
            std::string quadProxiesFileName = dataPath + "quads" + std::to_string(view) + ".bin.zstd";
            numProxies = quadBuffers->loadFromFile(quadProxiesFileName, &numBytes);
            totalBytesProxies += numBytes;

            std::string depthOffsetsFileName = dataPath + "depthOffsets" + std::to_string(view) + ".bin.zstd";
            numDepthOffsets = depthOffsets->loadFromFile(depthOffsetsFileName, &numBytes);
            totalBytesDepthOffsets += numBytes;

            // Create mesh
            Mesh* mesh = new Mesh({
                .maxVertices = numProxies * NUM_SUB_QUADS * VERTICES_IN_A_QUAD,
                .maxIndices = numProxies * NUM_SUB_QUADS * INDICES_IN_A_QUAD,
                .vertexSize = sizeof(QuadVertex),
                .attributes = QuadVertex::getVertexInputAttributes(),
                .material = new QuadMaterial({ .baseColorTexture = colorTexture }),
                .usage = GL_DYNAMIC_DRAW,
                .indirectDraw = true
            });
            meshes.push_back(mesh);

            // Generate the mesh data immediately
            auto& cameraToUse = (view == maxViews - 1) ? remoteCameraWideFov : remoteCamera;

            meshFromQuads->appendProxies(
                gBufferSize,
                numProxies,
                *quadBuffers
            );
            meshFromQuads->createMeshFromProxies(
                gBufferSize,
                numProxies, *depthOffsets,
                cameraToUse,
                *meshes[view]
            );

            Node* node = new Node(mesh);
            node->frustumCulled = false;
            node->setPosition(-1.0f * remoteCamera.getPosition());
            nodes.push_back(node);
            scene->addChildNode(node);

            Node* nodeWireframe = new Node(mesh);
            nodeWireframe->frustumCulled = false;
            nodeWireframe->wireframe = true;
            nodeWireframe->visible = false;
            nodeWireframe->primativeType = GL_LINES;
            nodeWireframe->overrideMaterial = new QuadMaterial({ .baseColor = colors[view % colors.size()] });
            nodeWireframe->setPosition(-1.0f * remoteCamera.getPosition());
            nodeWireframes.push_back(nodeWireframe);
            scene->addChildNode(nodeWireframe);

            spdlog::info("Loaded view {}: {} proxies ({} bytes), {} depth offsets ({} bytes)",
                        view, numProxies, numBytes,
                        numDepthOffsets, totalBytesDepthOffsets);
        }
    }

    void CreateActionSet() override {
        CreateAction(m_clickAction, "click-controller", XR_ACTION_TYPE_BOOLEAN_INPUT, {"/user/hand/left", "/user/hand/right"});
        CreateAction(m_thumbstickAction, "thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, {"/user/hand/left", "/user/hand/right"});
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
            OPENXR_CHECK(xrApplyHapticFeedback(m_session, &hapticActionInfo, (XrHapticBaseHeader*)&vibration),
                        "Failed to apply haptic feedback.");
        }
    }

    void HandleInteractions() override {
        for (int i = 0; i < 2; i++) {
            m_handNodes[i].visible = m_handPoseState[i].isActive;

            if (m_clickState[i].isActive == XR_TRUE &&
                m_clickState[i].currentState == XR_FALSE &&
                m_clickState[i].changedSinceLastSync == XR_TRUE) {
                XR_LOG("Click action triggered for hand: " << i);
                m_buzz[i] = 0.5f;

                for (int view = 0; view < maxViews; view++) {
                    nodeWireframes[view]->visible = !nodeWireframes[view]->visible;
                }
            }

            if (m_thumbstickState[i].isActive == XR_TRUE &&
                m_thumbstickState[i].changedSinceLastSync == XR_TRUE) {
                if (glm::abs(m_thumbstickState[i].currentState.x) > 0.2f ||
                    glm::abs(m_thumbstickState[i].currentState.y) > 0.2f) {
                    const glm::vec3 &forward = cameras.get()->left.getForwardVector();
                    const glm::vec3 &right = cameras.get()->left.getRightVector();
                    cameraPositionOffset += movementSpeed * forward * m_thumbstickState[i].currentState.y;
                    cameraPositionOffset += movementSpeed * right * m_thumbstickState[i].currentState.x;
                }
            }
        }
    }

    void OnRender(double now, double dt) override {
        auto start = std::chrono::high_resolution_clock::now();
        m_graphicsAPI->drawObjects(*scene.get(), *cameras.get());
        auto end = std::chrono::high_resolution_clock::now();

        spdlog::info("Rendering time: {:.3f}ms", std::chrono::duration<double, std::milli>(end - start).count());
    }

    void DestroyResources() override {
        delete meshFromQuads;
        delete quadBuffers;
        delete depthOffsets;

        for (auto texture : colorTextures) {
            delete texture;
        }
        for (auto mesh : meshes) {
            delete mesh;
        }
        for (auto node : nodes) {
            delete node;
        }
    }

private:
    PerspectiveCamera remoteCamera;
    PerspectiveCamera remoteCameraWideFov;

    // Single instances of shared resources
    MeshFromQuads* meshFromQuads;
    QuadBuffers* quadBuffers;
    DepthOffsets* depthOffsets;

    // Per-view resources
    std::vector<Texture*> colorTextures;
    std::vector<Mesh*> meshes;
    std::vector<Node*> nodes;
    std::vector<Node*> nodeWireframes;

    // Tracking data
    unsigned int numProxies = 0;
    unsigned int numDepthOffsets = 0;
    unsigned int totalBytesProxies = 0;
    unsigned int totalBytesDepthOffsets = 0;

    // XR Controller Actions
    XrAction m_clickAction;
    XrActionStateBoolean m_clickState[2] = {{XR_TYPE_ACTION_STATE_BOOLEAN}, {XR_TYPE_ACTION_STATE_BOOLEAN}};
    XrAction m_thumbstickAction;
    XrActionStateVector2f m_thumbstickState[2] = {{XR_TYPE_ACTION_STATE_VECTOR2F}, {XR_TYPE_ACTION_STATE_VECTOR2F}};
    float movementSpeed = 0.02f;
    XrAction m_buzzAction;
    float m_buzz[2] = {0, 0};
};

#endif
