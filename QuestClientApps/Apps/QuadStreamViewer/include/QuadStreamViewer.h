#ifndef QUAD_STREAM_RECEIVER_H
#define QUAD_STREAM_RECEIVER_H

#include <OpenXRApp.h>

#include <Path.h>
#include <Primitives/Mesh.h>
#include <Primitives/Model.h>
#include <Lights/AmbientLight.h>

#include <Quads/QuadMaterial.h>
#include <Quads/QuadsBuffers.h>
#include <Quads/DepthOffsets.h>
#include <Quads/MeshFromQuads.h>

using namespace quasar;

class QuadStreamViewer final : public OpenXRApp {
private:
    std::string sceneName = "robot_lab"; // choose from robot_lab, sun_temple, viking_village, or san_miguel
    std::string dataPathBase = "quads/" + sceneName + "/";

    uint maxAdditionalViews = 8;
    uint maxViews = maxAdditionalViews + 2; // +2 for primary and wide fov views
    float viewBoxSize = 0.5f;

    const std::vector<glm::vec3> offsets = {
        glm::vec3(-1.0f, +1.0f, -1.0f),
        glm::vec3(+1.0f, +1.0f, -1.0f),
        glm::vec3(+1.0f, -1.0f, -1.0f),
        glm::vec3(-1.0f, -1.0f, -1.0f),
        glm::vec3(-1.0f, +1.0f, +1.0f),
        glm::vec3(+1.0f, +1.0f, +1.0f),
        glm::vec3(+1.0f, -1.0f, +1.0f),
        glm::vec3(-1.0f, -1.0f, +1.0f),
    };

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
        glm::vec4(0.5f, 0.0f, 0.5f, 1.0f),
    };

public:
    QuadStreamViewer(GraphicsAPI_Type apiType)
        : OpenXRApp(apiType) {
        // Pre-allocate vectors
        remoteCameras.reserve(maxViews);
        meshes.reserve(maxViews);
        nodes.reserve(maxViews);
        nodeWireframes.reserve(maxViews);
        colorTextures.reserve(maxViews);
    }

    ~QuadStreamViewer() = default;

private:
    void CreateResources() override {
        scene->backgroundColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        scene->setAmbientLight(new AmbientLight({ .intensity = 1.0f }));

        // Add controller models
        Model* leftControllerMesh = new Model({
            .flipTextures = true,
            .IBL = 0.0f,
            .path = "models/quest-touch-plus-left.glb"
        });
        m_handNodes[0].setEntity(leftControllerMesh);

        Model* rightControllerMesh = new Model({
            .flipTextures = true,
            .IBL = 0.0f,
            .path = "models/quest-touch-plus-right.glb"
        });
        m_handNodes[1].setEntity(rightControllerMesh);

        Path dataPath = Path(dataPathBase);

        TextureFileCreateParams params = {
            .wrapS = GL_REPEAT,
            .wrapT = GL_REPEAT,
            .minFilter = GL_NEAREST,
            .magFilter = GL_NEAREST,
            .flipVertically = true,
            .gammaCorrected = true
        };
        for (int view = 0; view < maxViews; view++) {
            Path file = dataPath.appendToName("color" + std::to_string(view)).withExtension(".jpg");
            params.path = file;
            colorTextures.emplace_back(params);
        }

        glm::uvec2 remoteWindowSize = glm::uvec2(colorTextures[0].width, colorTextures[0].height);

        for (int view = 0; view < maxViews; view++) {
            remoteCameras[view] = new PerspectiveCamera(remoteWindowSize.x, remoteWindowSize.y);
            remoteCameras[view]->setFovyDegrees(90.0f);
            remoteCameras[view]->setPosition(glm::vec3(0.0f, 3.0f, 10.0f));
            remoteCameras[view]->updateViewMatrix();
        }
        PerspectiveCamera* remoteCameraCenter = remoteCameras[0];

        for (int view = 1; view < maxViews - 1; view++) {
            const glm::vec3& offset = offsets[view - 1];
            const glm::vec3& right = remoteCameraCenter->getRightVector();
            const glm::vec3& up = remoteCameraCenter->getUpVector();
            const glm::vec3& forward = remoteCameraCenter->getForwardVector();

            glm::vec3 worldOffset =
                right   * offset.x * viewBoxSize / 2.0f +
                up      * offset.y * viewBoxSize / 2.0f +
                forward * -offset.z * viewBoxSize / 2.0f;

            remoteCameras[view]->setViewMatrix(remoteCameraCenter->getViewMatrix());
            remoteCameras[view]->setPosition(remoteCameraCenter->getPosition() + worldOffset);
            remoteCameras[view]->updateViewMatrix();
        }

        remoteCameras[maxViews-1] = new PerspectiveCamera(remoteWindowSize.x, remoteWindowSize.y);
        remoteCameras[maxViews-1]->setFovyDegrees(120.0f);
        remoteCameras[maxViews-1]->setViewMatrix(remoteCameraCenter->getViewMatrix());

        meshFromQuads = new MeshFromQuads(remoteWindowSize);
        quadBuffers = new QuadBuffers(remoteWindowSize.x * remoteWindowSize.y * NUM_SUB_QUADS);
        depthOffsets = new DepthOffsets(2u * remoteWindowSize);

        // Load quad buffers and depth offsets
        for (int view = 0; view < maxViews; view++) {
            uint numBytes = 0;

            Path proxyFile = (dataPath / "quads").appendToName(std::to_string(view)).withExtension(".bin.zstd");
            uint numProxies = quadBuffers->loadFromFile(proxyFile, &numBytes);
            totalBytesProxies += numBytes;

            Path depthFile = (dataPath / "depthOffsets").appendToName(std::to_string(view)).withExtension(".bin.zstd");
            uint numOffsets = depthOffsets->loadFromFile(depthFile, &numBytes);
            totalBytesDepthOffsets += numBytes;

            totalDecompressTime += quadBuffers->stats.timeToDecompressMs;
            totalDecompressTime += depthOffsets->stats.timeToDecompressMs;

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

            meshFromQuads->appendQuads(
                gBufferSize,
                numProxies,
                *quadBuffers
            );
            meshFromQuads->createMeshFromProxies(
                gBufferSize,
                numProxies, *depthOffsets,
                *remoteCameras[view],
                *meshes[view]
            );

            totalProxies += numProxies;
            totalDepthOffsets = numOffsets;
        }

        // Create nodes
        for (int view = 0; view < maxViews; view++) {
            nodes[view] = new Node(meshes[view]);
            nodes[view]->frustumCulled = false;
            nodes[view]->setPosition(-1.0f * remoteCameraCenter->getPosition());
            scene->addChildNode(nodes[view]);

            nodeWireframes[view] = new Node(meshes[view]);
            nodeWireframes[view]->frustumCulled = false;
            nodeWireframes[view]->wireframe = true;
            nodeWireframes[view]->visible = false;
            nodeWireframes[view]->primativeType = GL_LINES;
            nodeWireframes[view]->overrideMaterial = new QuadMaterial({ .baseColor = colors[view % colors.size()] });
            nodeWireframes[view]->setPosition(-1.0f * remoteCameraCenter->getPosition());
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

        for (auto* c : remoteCameras) delete c;
        for (auto* m : meshes) delete m;
        for (auto* n : nodes) delete n;
        for (auto* n : nodeWireframes) delete n;
    }

private:
    std::vector<PerspectiveCamera*> remoteCameras;

    MeshFromQuads* meshFromQuads = nullptr;
    QuadBuffers* quadBuffers = nullptr;
    DepthOffsets* depthOffsets = nullptr;

    std::vector<Texture> colorTextures;
    std::vector<Mesh*> meshes;
    std::vector<Node*> nodes;
    std::vector<Node*> nodeWireframes;

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

#endif // QUAD_STREAM_RECEIVER_H
