#ifndef QUAD_STREAM_RECEIVER_H
#define QUAD_STREAM_RECEIVER_H

#include <OpenXRApp.h>

#include <Path.h>
#include <Primitives/Mesh.h>
#include <Primitives/Model.h>

#include <Lights/AmbientLight.h>

#include <Quads/QuadFrames.h>
#include <Quads/QuadMesh.h>

using namespace quasar;

class QuadStreamViewer final : public OpenXRApp {
private:
    std::string sceneName = "robot_lab"; // choose from robot_lab, sun_temple, viking_village, or san_miguel
    Path dataPath = Path("quads/" + sceneName);

    uint maxAdditionalViews = 8;
    uint maxViews = maxAdditionalViews + 2; // +2 for primary and wide fov views
    float viewBoxSize = 0.5f;

public:
    QuadStreamViewer(GraphicsAPI_Type apiType)
        : OpenXRApp(apiType)
    {
        // Pre-allocate vectors
        remoteCameras.reserve(maxViews);
        colorTextures.reserve(maxViews);
        meshes.reserve(maxViews);
        nodes.reserve(maxViews);
        nodeWireframes.reserve(maxViews);
        frames.resize(maxViews);
    }

    ~QuadStreamViewer() = default;

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
        handNodes[0].setEntity(leftControllerModel.get());

        rightControllerModel = std::make_unique<Model>(ModelCreateParams{
            .flipTextures = true,
            .IBL = 0.0f,
            .path = "models/quest-touch-plus-right.glb"
        });
        handNodes[1].setEntity(rightControllerModel.get());

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
            Path file = dataPath.appendToName("color" + std::to_string(view)).withExtension(".jpg");
            params.path = file;
            colorTextures.emplace_back(params);
            frames.emplace_back();
        }

        remoteGBufferSize = glm::uvec2(colorTextures[0].width, colorTextures[0].height);

        for (int view = 0; view < maxViews; view++) {
            remoteCameras.emplace_back(remoteGBufferSize.x, remoteGBufferSize.y);
            remoteCameras[view].setFovyDegrees(90.0f);
            remoteCameras[view].setPosition(glm::vec3(0.0f, 3.0f, 10.0f));
            remoteCameras[view].updateViewMatrix();
        }
        PerspectiveCamera& remoteCameraCenter = remoteCameras[0];

        for (int view = 1; view < maxViews - 1; view++) {
            const glm::vec3& offset = offsets[view - 1];
            const glm::vec3& right = remoteCameraCenter.getRightVector();
            const glm::vec3& up = remoteCameraCenter.getUpVector();
            const glm::vec3& forward = remoteCameraCenter.getForwardVector();

            glm::vec3 worldOffset =
                right   * offset.x * viewBoxSize / 2.0f +
                up      * offset.y * viewBoxSize / 2.0f +
                forward * -offset.z * viewBoxSize / 2.0f;

            remoteCameras[view].setViewMatrix(remoteCameraCenter.getViewMatrix());
            remoteCameras[view].setPosition(remoteCameraCenter.getPosition() + worldOffset);
            remoteCameras[view].updateViewMatrix();
        }

        remoteCameras[maxViews-1] = PerspectiveCamera(remoteGBufferSize.x, remoteGBufferSize.y);
        remoteCameras[maxViews-1].setFovyDegrees(120.0f);
        remoteCameras[maxViews-1].setViewMatrix(remoteCameraCenter.getViewMatrix());

        quadSet = std::make_unique<QuadSet>(remoteGBufferSize);

        QuadSet::Sizes totalSizes;

        // Load quad buffers and depth offsets
        for (int view = 0; view < maxViews; view++) {
            double startTime = timeutils::getTimeMicros();
            frames[view].loadFromFiles(dataPath, view);
            loadFromFilesTime = timeutils::microsToMillis(timeutils::getTimeMicros() - startTime);

            startTime = timeutils::getTimeMicros();
            auto sizes = quadSet->unmapFromCPU(frames[view].quads, frames[view].depthOffsets);
            transferTime = timeutils::microsToMillis(quadSet->stats.timeToTransferMs);

            // Create mesh
            meshes.emplace_back(*quadSet, colorTextures[view], sizes.numQuads);

            const glm::vec2& gBufferSize = glm::vec2(colorTextures[view].width, colorTextures[view].height);
            meshes[view].appendQuads(*quadSet, gBufferSize);
            meshes[view].createMeshFromProxies(*quadSet, gBufferSize, remoteCameras[view]);

            totalSizes += sizes;
        }

        // Create nodes
        for (int view = 0; view < maxViews; view++) {
            nodes.emplace_back(&meshes[view]);
            nodes[view].frustumCulled = false;
            nodes[view].setPosition(-1.0f * remoteCameraCenter.getPosition());
            scene->addChildNode(&nodes[view]);

            nodeWireframes.emplace_back(&meshes[view]);
            nodeWireframes[view].frustumCulled = false;
            nodeWireframes[view].wireframe = true;
            nodeWireframes[view].visible = false;
            nodeWireframes[view].primativeType = GL_LINES;
            nodeWireframes[view].overrideMaterial = new QuadMaterial({ .baseColor = colors[view % colors.size()] });
            nodeWireframes[view].setPosition(-1.0f * remoteCameraCenter.getPosition());
            scene->addChildNode(&nodeWireframes[view]);
        }

        spdlog::info("Time to load from files: {:.3f}ms", loadFromFilesTime);
        spdlog::info("Time to transfer to GPU: {:.3f}ms", transferTime);
        spdlog::info("Loaded {} quads ({:.3f} MB), {} depth offsets ({:.3f} MB)",
                     totalSizes.numQuads, totalSizes.quadsSize / BYTES_PER_MEGABYTE,
                     totalSizes.numDepthOffsets, totalSizes.depthOffsetsSize / BYTES_PER_MEGABYTE);
    }

    void CreateActionSet() override {
        // An Action for clicking on the controller.
        CreateAction(clickAction, "click-controller", XR_ACTION_TYPE_BOOLEAN_INPUT, {"/user/hand/left", "/user/hand/right"});
        // An Action for the position of the thumbstick.
        CreateAction(thumbstickAction, "thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, {"/user/hand/left", "/user/hand/right"});
        // An Action for a vibration output on one or other hand.
        CreateAction(buzzAction, "buzz", XR_ACTION_TYPE_VIBRATION_OUTPUT, {"/user/hand/left", "/user/hand/right"});
    }

    void SuggestBindings(std::map<std::string, std::vector<XrActionSuggestedBinding>>& bindings) override {
        bindings["/interaction_profiles/khr/simple_controller"].push_back({clickAction, CreateXrPath("/user/hand/left/input/select/click")});
        bindings["/interaction_profiles/khr/simple_controller"].push_back({clickAction, CreateXrPath("/user/hand/right/input/select/click")});
        bindings["/interaction_profiles/khr/simple_controller"].push_back({buzzAction, CreateXrPath("/user/hand/left/output/haptic")});
        bindings["/interaction_profiles/khr/simple_controller"].push_back({buzzAction, CreateXrPath("/user/hand/right/output/haptic")});

        bindings["/interaction_profiles/oculus/touch_controller"].push_back({clickAction, CreateXrPath("/user/hand/left/input/trigger/value")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({clickAction, CreateXrPath("/user/hand/right/input/trigger/value")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({thumbstickAction, CreateXrPath("/user/hand/left/input/thumbstick")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({thumbstickAction, CreateXrPath("/user/hand/right/input/thumbstick")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({buzzAction, CreateXrPath("/user/hand/left/output/haptic")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({buzzAction, CreateXrPath("/user/hand/right/output/haptic")});
    }

    void PollActions(XrTime predictedTime) override {
        XrActionStateGetInfo actionStateGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};

        for (int i = 0; i < 2; i++) {
            actionStateGetInfo.action = clickAction;
            actionStateGetInfo.subactionPath = handPaths[i];
            OPENXR_CHECK(xrGetActionStateBoolean(session, &actionStateGetInfo, &clickState[i]),
                        "Failed to get Boolean State of Click action.");

            actionStateGetInfo.action = thumbstickAction;
            actionStateGetInfo.subactionPath = handPaths[i];
            OPENXR_CHECK(xrGetActionStateVector2f(session, &actionStateGetInfo, &thumbstickState[i]),
                        "Failed to get Vector2f State of Thumbstick action.");

            buzz[i] *= 0.5f;
            if (buzz[i] < 0.01f)
                buzz[i] = 0.0f;
            XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
            vibration.amplitude = buzz[i];
            vibration.duration = XR_MIN_HAPTIC_DURATION;
            vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

            XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
            hapticActionInfo.action = buzzAction;
            hapticActionInfo.subactionPath = handPaths[i];
            OPENXR_CHECK(xrApplyHapticFeedback(session, &hapticActionInfo, (XrHapticBaseHeader*)&vibration),
                        "Failed to apply haptic feedback.");
        }
    }

    void HandleInteractions() override {
        // For each hand:
        for (int i = 0; i < 2; i++) {
            // Draw the controllers:
            handNodes[i].visible = handPoseState[i].isActive;

            if (clickState[i].isActive == XR_TRUE &&
                clickState[i].currentState == XR_FALSE &&
                clickState[i].changedSinceLastSync == XR_TRUE) {
                // XR_LOG("Click action triggered for hand: " << i);
                buzz[i] = 0.5f;

                for (int view = 0; view < maxViews; view++) {
                    nodeWireframes[view].visible = !nodeWireframes[view].visible;
                }
            }

            if (thumbstickState[i].isActive == XR_TRUE &&
                thumbstickState[i].changedSinceLastSync == XR_TRUE) {
                if (glm::abs(thumbstickState[i].currentState.x) > 0.2f ||
                    glm::abs(thumbstickState[i].currentState.y) > 0.2f) {
                    const glm::vec3 &forward = cameras->left.getForwardVector();
                    const glm::vec3 &right = cameras->left.getRightVector();
                    cameraPositionOffset += movementSpeed * forward * thumbstickState[i].currentState.y;
                    cameraPositionOffset += movementSpeed * right * thumbstickState[i].currentState.x;
                }
            }
        }
    }

    void OnRender(double now, double dt) override {
        graphicsAPI->drawObjects(*scene, *cameras);
        spdlog::info("Rendering time: {:.3f}ms", timeutils::secondsToMillis(dt));
    }

    void DestroyResources() override {}

private:
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

    double loadFromFilesTime = 0.0;
    double transferTime = 0.0;

    glm::uvec2 remoteGBufferSize;
    std::vector<PerspectiveCamera> remoteCameras;

    std::unique_ptr<QuadSet> quadSet;

    std::vector<Texture> colorTextures;
    std::vector<QuadMesh> meshes;
    std::vector<Node> nodes;
    std::vector<Node> nodeWireframes;
    std::vector<ReferenceFrame> frames;

    std::unique_ptr<Model> leftControllerModel;
    std::unique_ptr<Model> rightControllerModel;

    // Actions.
    XrAction clickAction;
    // The realtime states of these actions.
    XrActionStateBoolean clickState[2] = {{XR_TYPE_ACTION_STATE_BOOLEAN}, {XR_TYPE_ACTION_STATE_BOOLEAN}};
    // The thumbstick input action.
    XrAction thumbstickAction;
    // The current thumbstick state for each controller.
    XrActionStateVector2f thumbstickState[2] = {{XR_TYPE_ACTION_STATE_VECTOR2F}, {XR_TYPE_ACTION_STATE_VECTOR2F}};
    float movementSpeed = 0.03f;
    // The haptic output action for grabbing cubes.
    XrAction buzzAction;
    // The current haptic output value for each controller.
    float buzz[2] = {0, 0};
};

#endif // QUAD_STREAM_RECEIVER_H
