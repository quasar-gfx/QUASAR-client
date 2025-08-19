#ifndef QUASAR_VIEWER_H
#define QUASAR_VIEWER_H

#include <OpenXRApp.h>

#include <Path.h>
#include <Primitives/Mesh.h>
#include <Primitives/Model.h>

#include <Lights/AmbientLight.h>

#include <Quads/QuadFrames.h>
#include <Quads/QuadMesh.h>

using namespace quasar;

class QUASARViewer final : public OpenXRApp {
private:
    std::string sceneName = "robot_lab"; // choose from robot_lab, sun_temple, viking_village, or san_miguel
    Path dataPath = Path("quads/" + sceneName);

    uint numHiddenLayers = 3;
    uint maxLayers = numHiddenLayers + 2; // add visible and wide FOV layer

public:
    QUASARViewer(GraphicsAPI_Type apiType)
        : OpenXRApp(apiType)
    {
        // Pre-allocate vectors
        colorTextures.reserve(maxLayers);
        meshes.reserve(maxLayers);
        nodes.reserve(maxLayers);
        nodeWireframes.reserve(maxLayers);
        frames.resize(maxLayers);
    }
    ~QUASARViewer() = default;

private:
    void CreateResources() override {
        scene->backgroundColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        scene->setAmbientLight(new AmbientLight({ .intensity = 1.0f }));

        // Add controller models
        leftControllerModel = std::make_unique<Model>(ModelCreateParams{
            .flipTextures = true,
            .IBL = 0.0f,
            .path = "models/quest-touch-plus-left.glb"
        });
        m_handNodes[0].setEntity(leftControllerModel.get());

        rightControllerModel = std::make_unique<Model>(ModelCreateParams{
            .flipTextures = true,
            .IBL = 0.0f,
            .path = "models/quest-touch-plus-right.glb"
        });
        m_handNodes[1].setEntity(rightControllerModel.get());

        // Load all textures
        TextureFileCreateParams params = {
            .wrapS = GL_REPEAT,
            .wrapT = GL_REPEAT,
            .minFilter = GL_NEAREST,
            .magFilter = GL_NEAREST,
            .flipVertically = true,
            .gammaCorrected = true
        };
        for (int layer = 0; layer < maxLayers; layer++) {
            // Load color texture
            Path colorFileName = dataPath.appendToName("color" + std::to_string(layer));
            params.path = colorFileName.withExtension(".jpg");
            colorTextures.emplace_back(params);
            frames.emplace_back();
        }

        remoteGBufferSize = glm::uvec2(colorTextures[0].width, colorTextures[0].height);

        remoteCamera = new PerspectiveCamera(remoteGBufferSize.x, remoteGBufferSize.y);
        remoteCameraWideFov = new PerspectiveCamera(remoteGBufferSize.x, remoteGBufferSize.y);
        remoteCamera->setFovyDegrees(90.0f);
        remoteCameraWideFov->setFovyDegrees(120.0f);

        remoteCamera->setPosition(glm::vec3(0.0f, 3.0f, 10.0f));
        remoteCamera->updateViewMatrix();
        remoteCameraWideFov->setViewMatrix(remoteCamera->getViewMatrix());

        quadSet = std::make_unique<QuadSet>(remoteGBufferSize);

        QuadSet::Sizes totalSizes;

        // Load quad buffers and depth offsets
        for (int layer = 0; layer < maxLayers; layer++) {
            double startTime = timeutils::getTimeMicros();
            frames[layer].loadFromFiles(dataPath, layer);
            loadFromFilesTime = timeutils::microsToMillis(timeutils::getTimeMicros() - startTime);

            startTime = timeutils::getTimeMicros();
            auto sizes = quadSet->unmapFromCPU(frames[layer].quads, frames[layer].depthOffsets);
            transferTime = timeutils::microsToMillis(quadSet->stats.timeToTransferMs);

            // Create mesh
            meshes.emplace_back(*quadSet, colorTextures[layer], sizes.numQuads);

            auto* cameraToUse = (layer == maxLayers - 1) ? remoteCameraWideFov : remoteCamera;
            const glm::vec2& gBufferSize = glm::vec2(colorTextures[layer].width, colorTextures[layer].height);
            meshes[layer].appendQuads(*quadSet, gBufferSize);
            meshes[layer].createMeshFromProxies(*quadSet, gBufferSize, *cameraToUse);

            totalSizes += sizes;
        }

        // Create nodes
        for (int layer = 0; layer < maxLayers; layer++) {
            nodes.emplace_back(&meshes[layer]);
            nodes[layer].frustumCulled = false;
            nodes[layer].setPosition(-1.0f * remoteCamera->getPosition());
            scene->addChildNode(&nodes[layer]);

            nodeWireframes.emplace_back(&meshes[layer]);
            nodeWireframes[layer].frustumCulled = false;
            nodeWireframes[layer].wireframe = true;
            nodeWireframes[layer].visible = false;
            nodeWireframes[layer].primativeType = GL_LINES;
            nodeWireframes[layer].overrideMaterial = new QuadMaterial({ .baseColor = colors[layer % colors.size()] });
            nodeWireframes[layer].setPosition(-1.0f * remoteCamera->getPosition());
            scene->addChildNode(&nodeWireframes[layer]);
        }

        spdlog::info("Time to load from files: {:.3f}ms", loadFromFilesTime);
        spdlog::info("Time to transfer to GPU: {:.3f}ms", transferTime);
        spdlog::info("Loaded {} quads ({:.3f} MB), {} depth offsets ({:.3f} MB)",
                     totalSizes.numQuads, totalSizes.quadsSize / BYTES_PER_MEGABYTE,
                     totalSizes.numDepthOffsets, totalSizes.depthOffsetsSize / BYTES_PER_MEGABYTE);
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

                for (int layer = 0; layer < maxLayers; layer++) {
                    nodeWireframes[layer].visible = !nodeWireframes[layer].visible;
                }
            }

            if (m_thumbstickState[i].isActive == XR_TRUE &&
                m_thumbstickState[i].changedSinceLastSync == XR_TRUE) {
                if (glm::abs(m_thumbstickState[i].currentState.x) > 0.2f ||
                    glm::abs(m_thumbstickState[i].currentState.y) > 0.2f) {
                    const glm::vec3 &forward = cameras->left.getForwardVector();
                    const glm::vec3 &right = cameras->left.getRightVector();
                    cameraPositionOffset += movementSpeed * forward * m_thumbstickState[i].currentState.y;
                    cameraPositionOffset += movementSpeed * right * m_thumbstickState[i].currentState.x;
                }
                // XR_LOG("Thumbstick action triggered for hand: " << i << " with value: " << m_thumbstickState[i].currentState.x << ", " << m_thumbstickState[i].currentState.y);
            }
        }
    }

    void OnRender(double now, double dt) override {
        m_graphicsAPI->drawObjects(*scene, *cameras);
        spdlog::info("Rendering time: {:.3f}ms", timeutils::secondsToMillis(dt));
    }

    void DestroyResources() override {}

private:
    const std::vector<glm::vec4> colors = {
        glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), // primary layer color is yellow
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

    double loadFromFilesTime = 0.0;
    double transferTime = 0.0;

    glm::uvec2 remoteGBufferSize;
    PerspectiveCamera* remoteCamera;
    PerspectiveCamera* remoteCameraWideFov;

    std::unique_ptr<QuadSet> quadSet;

    std::vector<Texture> colorTextures;
    std::vector<QuadMesh> meshes;
    std::vector<Node> nodes;
    std::vector<Node> nodeWireframes;
    std::vector<ReferenceFrame> frames;

    std::unique_ptr<Model> leftControllerModel;
    std::unique_ptr<Model> rightControllerModel;

    // Actions.
    XrAction m_clickAction;
    // The realtime states of these actions.
    XrActionStateBoolean m_clickState[2] = {{XR_TYPE_ACTION_STATE_BOOLEAN}, {XR_TYPE_ACTION_STATE_BOOLEAN}};
    // The thumbstick input action.
    XrAction m_thumbstickAction;
    // The current thumbstick state for each controller.
    XrActionStateVector2f m_thumbstickState[2] = {{XR_TYPE_ACTION_STATE_VECTOR2F}, {XR_TYPE_ACTION_STATE_VECTOR2F}};
    float movementSpeed = 0.03f;
    // The haptic output action for grabbing cubes.
    XrAction m_buzzAction;
    // The current haptic output value for each controller.
    float m_buzz[2] = {0, 0};
};

#endif // QUASAR_VIEWER_H
