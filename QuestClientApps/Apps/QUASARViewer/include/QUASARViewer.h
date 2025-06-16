#ifndef DP_VIEWER_H
#define DP_VIEWER_H

#include <OpenXRApp.h>

#include <Path.h>
#include <Primitives/Mesh.h>
#include <Primitives/Model.h>
#include <Primitives/Cube.h>

#include <Lights/AmbientLight.h>

#include <Quads/QuadMaterial.h>
#include <Quads/QuadsBuffers.h>
#include <Quads/DepthOffsets.h>
#include <Quads/MeshFromQuads.h>

using namespace quasar;

class QUASARViewer final : public OpenXRApp {
private:
    std::string sceneName = "robot_lab";
    std::string dataPathBase = "quads/" + sceneName + "/";

    uint maxLayers = 4;
    uint maxViews = maxLayers + 1;

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
    QUASARViewer(GraphicsAPI_Type apiType)
            : OpenXRApp(apiType) {
        // Pre-allocate vectors
        meshes.reserve(maxViews);
        nodes.reserve(maxViews);
        nodeWireframes.reserve(maxViews);
        colorTextures.reserve(maxViews);
    }
    ~QUASARViewer() = default;

private:
    void CreateResources() override {
        scene->backgroundColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        AmbientLight* ambientLight = new AmbientLight({
            .intensity = 1.0f
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

        Path dataPath = Path(dataPathBase);

        // Load all textures
        TextureFileCreateParams params = {
            .wrapS = GL_REPEAT,
            .wrapT = GL_REPEAT,
            .minFilter = GL_NEAREST,
            .magFilter = GL_NEAREST,
            .flipVertically = true,
            .gammaCorrected = true
        };
        for (int view = 0; view < maxViews; view++) {
            // Load color texture
            Path colorFileName = dataPath.appendToName("color" + std::to_string(view));
            params.path = colorFileName.withExtension(".jpg");
            colorTextures.emplace_back(params);
        }

        glm::uvec2 remoteWindowSize = glm::uvec2(colorTextures[0].width, colorTextures[0].height);

        remoteCamera = new PerspectiveCamera(remoteWindowSize.x, remoteWindowSize.y);
        remoteCameraWideFov = new PerspectiveCamera(remoteWindowSize.x, remoteWindowSize.y);
        remoteCamera->setFovyDegrees(90.0f);
        remoteCameraWideFov->setFovyDegrees(120.0f);

        remoteCamera->setPosition(glm::vec3(0.0f, 3.0f, 10.0f));
        remoteCamera->updateViewMatrix();
        remoteCameraWideFov->setViewMatrix(remoteCamera->getViewMatrix());

        // Create shared instances - only one of each!
        meshFromQuads = new MeshFromQuads(remoteWindowSize);

        uint maxProxies = remoteWindowSize.x * remoteWindowSize.y * NUM_SUB_QUADS;
        quadBuffers = new QuadBuffers(maxProxies);

        const glm::uvec2 depthBufferSize = 2u * remoteWindowSize;
        depthOffsets = new DepthOffsets(depthBufferSize);

        // Load quad buffers and depth offsets
        for (int view = 0; view < maxViews; view++) {
            uint numBytes;

            // Load proxy data
            Path quadProxiesFileName = (dataPath / "quads").appendToName(std::to_string(view)).withExtension(".bin.zstd");
            uint numProxies = quadBuffers->loadFromFile(quadProxiesFileName, &numBytes);
            totalBytesProxies += numBytes;

            Path depthOffsetsFileName = (dataPath / "depthOffsets").appendToName(std::to_string(view)).withExtension(".bin.zstd");
            uint numDepthOffsets = depthOffsets->loadFromFile(depthOffsetsFileName, &numBytes);
            totalBytesDepthOffsets += numBytes;

            totalDecompressTime = quadBuffers->stats.timeToDecompressMs;
            totalDecompressTime += depthOffsets->stats.timeToDecompressMs;

            // Create mesh
            meshes[view] = new Mesh({
                .maxVertices = numProxies * NUM_SUB_QUADS * VERTICES_IN_A_QUAD,
                .maxIndices = numProxies * NUM_SUB_QUADS * INDICES_IN_A_QUAD,
                .vertexSize = sizeof(QuadVertex),
                .attributes = QuadVertex::getVertexInputAttributes(),
                .material = new QuadMaterial({ .baseColorTexture = &colorTextures[view] }),
                .usage = GL_DYNAMIC_DRAW,
                .indirectDraw = true
            });

            const glm::uvec2 gBufferSize = glm::uvec2(colorTextures[view].width, colorTextures[view].height);

            auto* cameraToUse = (view == maxViews - 1) ? remoteCameraWideFov : remoteCamera;
            meshFromQuads->appendQuads(
                gBufferSize,
                numProxies,
                *quadBuffers
            );
            meshFromQuads->createMeshFromProxies(
                gBufferSize,
                numProxies, *depthOffsets,
                *cameraToUse,
                *meshes[view]
            );

            totalProxies += numProxies;
            totalDepthOffsets = numDepthOffsets;
        }

        // Create nodes
        for (int view = 0; view < maxViews; view++) {
            nodes[view] = new Node(meshes[view]);
            nodes[view]->frustumCulled = false;
            nodes[view]->setPosition(-1.0f * remoteCamera->getPosition());
            scene->addChildNode(nodes[view]);

            nodeWireframes[view] = new Node(meshes[view]);
            nodeWireframes[view]->frustumCulled = false;
            nodeWireframes[view]->wireframe = true;
            nodeWireframes[view]->visible = false;
            nodeWireframes[view]->primativeType = GL_LINES;
            nodeWireframes[view]->overrideMaterial = new QuadMaterial({ .baseColor = colors[view % colors.size()] });
            nodeWireframes[view]->setPosition(-1.0f * remoteCamera->getPosition());
            scene->addChildNode(nodeWireframes[view]);
        }

        spdlog::info("Decompress time: {:.3f}ms", totalDecompressTime);
        spdlog::info("Loaded {} proxies ({:.3f} MB), {} depth offsets ({:.3f} MB)",
                        totalProxies, static_cast<float>(totalBytesProxies) / BYTES_IN_MB,
                        totalDepthOffsets, static_cast<float>(totalBytesDepthOffsets) / BYTES_IN_MB);
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
                // XR_LOG("Click action triggered for hand: " << i);
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
        m_graphicsAPI->drawObjects(*scene.get(), *cameras.get());
        spdlog::info("Rendering time: {:.3f}ms", timeutils::secondsToMillis(dt));
    }

    void DestroyResources() override {
        delete meshFromQuads;
        delete quadBuffers;
        delete depthOffsets;

        for (auto mesh : meshes) {
            delete mesh;
        }
        for (auto node : nodes) {
            delete node;
        }
    }

private:
    PerspectiveCamera* remoteCamera;
    PerspectiveCamera* remoteCameraWideFov;

    // Single instances of shared resources
    MeshFromQuads* meshFromQuads;
    QuadBuffers* quadBuffers;
    DepthOffsets* depthOffsets;

    // Per-view resources
    std::vector<Texture> colorTextures;
    std::vector<Mesh*> meshes;
    std::vector<Node*> nodes;
    std::vector<Node*> nodeWireframes;

    // Tracking data
    uint totalProxies = 0;
    uint totalDepthOffsets = 0;
    uint totalBytesProxies = 0;
    uint totalBytesDepthOffsets = 0;

    double totalDecompressTime = 0.0;

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
