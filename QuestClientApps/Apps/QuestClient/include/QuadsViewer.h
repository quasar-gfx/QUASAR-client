#ifndef QUADS_VIEWER_H
#define QUADS_VIEWER_H

#include <OpenXRApp.h>

#include <Primitives/Mesh.h>
#include <Primitives/Model.h>
#include <Primitives/Cube.h>

#include <Materials/UnlitMaterial.h>
#include <Lights/AmbientLight.h>

#include <Quads/QuadMaterial.h>
#include <Quads/QuadsBuffers.h>
#include <Quads/DepthOffsets.h>
#include <Quads/MeshFromQuads.h>

class QuadsViewer final : public OpenXRApp {
private:
    std::string dataPath = "quadwarp/";

    glm::uvec2 windowSize = glm::uvec2(1920, 1080);
    float loadProxies = true;

public:
    QuadsViewer(GraphicsAPI_Type apiType) : OpenXRApp(apiType), remoteCamera(windowSize.x, windowSize.y) {}
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
            .path = "models/quest-touch-plus-left.glb"
        });
        m_handNodes[0].setEntity(leftControllerMesh);

        Model* rightControllerMesh = new Model({
            .flipTextures = true,
            .IBL = 0,
            .path = "models/quest-touch-plus-right.glb"
        });
        m_handNodes[1].setEntity(rightControllerMesh);

        remoteCamera.setPosition(glm::vec3(0.0f, 3.0f, 10.0f));
        remoteCamera.updateViewMatrix();

        meshFromQuads = new MeshFromQuads(windowSize);

        std::string colorFileName = dataPath + "color.png";
        colorTexture = new Texture({
            .wrapS = GL_REPEAT,
            .wrapT = GL_REPEAT,
            .minFilter = GL_NEAREST,
            .magFilter = GL_NEAREST,
            .flipVertically = true,
            .path = colorFileName
        });

        // // add a screen for the texture.
        // Cube* videoScreen = new Cube({
        //     .material = new UnlitMaterial({ .baseColorTexture = colorTexture }),
        // });
        // Node* screen = new Node(videoScreen);
        // screen->setPosition(glm::vec3(0.0f, 0.0f, -2.0f));
        // screen->setScale(glm::vec3(1.0f, 0.5f, 0.01f));
        // screen->frustumCulled = false;
        // scene->addChildNode(screen);

        if (!loadProxies) {
            std::string verticesFileName = dataPath + "vertices.bin";
            std::string indicesFileName = dataPath + "indices.bin";

            auto vertexData = FileIO::loadBinaryFile(verticesFileName);
            auto indexData = FileIO::loadBinaryFile(indicesFileName);

            std::vector<Vertex> vertices(vertexData.size() / sizeof(Vertex));
            std::memcpy(vertices.data(), vertexData.data(), vertexData.size());

            std::vector<unsigned int> indices(indexData.size() / sizeof(unsigned int));
            std::memcpy(indices.data(), indexData.data(), indexData.size());

            mesh = new Mesh({
                .vertices = vertices,
                .indices = indices,
                .material = new QuadMaterial({ .baseColorTexture = colorTexture })
            });
        }
        else {
            unsigned int maxProxies = windowSize.x * windowSize.y * NUM_SUB_QUADS;
            quadBuffers = new QuadBuffers(maxProxies);

            const glm::uvec2 depthBufferSize = 2u * windowSize;
            depthOffsets = new DepthOffsets(depthBufferSize);

            std::string quadProxiesFileName = dataPath + "quads.bin";
            std::string depthOffsetsFileName = dataPath + "depthOffsets.bin";

            // load the quad proxies
            numProxies = quadBuffers->loadFromFile(quadProxiesFileName);
            // load depth offsets
            numDepthOffsets = depthOffsets->loadFromFile(depthOffsetsFileName);

            mesh = new Mesh({
                .numVertices = numProxies * NUM_SUB_QUADS * VERTICES_IN_A_QUAD,
                .numIndices = numProxies * NUM_SUB_QUADS * INDICES_IN_A_QUAD,
                .material = new QuadMaterial({ .baseColorTexture = colorTexture }),
                .usage = GL_DYNAMIC_DRAW,
                .indirectDraw = true
            });

            spdlog::info("Loaded {} proxies and {} depth offsets", numProxies, numDepthOffsets);
        }

        node = new Node(mesh);
        node->frustumCulled = false;
        node->setPosition(-1.0f * remoteCamera.getPosition());
        scene->addChildNode(node);
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
            OPENXR_CHECK(xrGetActionStateBoolean(m_session, &actionStateGetInfo, &m_clickState[i]), "Failed to get Boolean State of Click action.");

            actionStateGetInfo.action = m_thumbstickAction;
            actionStateGetInfo.subactionPath = m_handPaths[i];
            OPENXR_CHECK(xrGetActionStateVector2f(m_session, &actionStateGetInfo, &m_thumbstickState[i]), "Failed to get Vector2f State of Thumbstick action.");

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
        meshFromQuads->appendProxies(
            windowSize,
            numProxies,
            *quadBuffers
        );
        meshFromQuads->createMeshFromProxies(
            windowSize,
            numProxies, *depthOffsets,
            remoteCamera,
            *mesh
        );

        auto start = std::chrono::high_resolution_clock::now();
        auto renderStats = m_graphicsAPI->drawObjects(*scene.get(), *cameras.get());
        auto end = std::chrono::high_resolution_clock::now();

        spdlog::info("Time to append proxies: {:.3f}ms", meshFromQuads->stats.timeToAppendProxiesMs);
        spdlog::info("Time to fill output quads: {:.3f}ms", meshFromQuads->stats.timeToFillOutputQuadsMs);
        spdlog::info("Time to create mesh: {:.3f}ms", meshFromQuads->stats.timeToCreateMeshMs);
        spdlog::info("Rendering time: {:.3f}ms", std::chrono::duration<double, std::milli>(end - start).count());
    }

    void DestroyResources() override {
        delete meshFromQuads;
        delete colorTexture;
        delete mesh;
        delete node;
    }

    PerspectiveCamera remoteCamera;

    MeshFromQuads* meshFromQuads;

    unsigned int numProxies;
    unsigned int numDepthOffsets;

    QuadBuffers* quadBuffers;
    DepthOffsets* depthOffsets;

    Mesh* mesh;
    Texture* colorTexture;
    Node* node;

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

#endif // QUADS_VIEWER_H
