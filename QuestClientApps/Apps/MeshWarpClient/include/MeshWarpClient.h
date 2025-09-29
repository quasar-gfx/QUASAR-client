#ifndef MESHWARP_CLIENT_H
#define MESHWARP_CLIENT_H

#include <OpenXRApp.h>

#include <Primitives/Mesh.h>
#include <Primitives/Cube.h>
#include <Primitives/Model.h>
#include <Materials/UnlitMaterial.h>
#include <Lights/AmbientLight.h>

#include <Cameras/PerspectiveCamera.h>
#include <Utils/FileIO.h>

#include <Receivers/VideoTexture.h>
#include <Receivers/BC4DepthVideoTexture.h>
#include <Streamers/PoseStreamer.h>

#include <shaders_common.h>

#define GEN_MESH_THREADS_PER_LOCALGROUP 16

using namespace quasar;

class MeshWarpClient final : public OpenXRApp {
private:
    std::string serverIP = "192.168.22.227";
    std::string poseURL = serverIP + ":54321";
    std::string videoURL = "0.0.0.0:12345";
    std::string depthURL = serverIP + ":65432";

    const glm::uvec2 videoSize = glm::uvec2(1920, 1080);

    unsigned int surfelSize = 1;
    unsigned int depthFactor = 4;
    float remoteFOV = 120.0f;

public:
    MeshWarpClient(GraphicsAPI_Type apiType)
        : OpenXRApp(apiType)
        , remoteCamera(videoSize)
    {}
    ~MeshWarpClient() = default;

private:
    void CreateResources() override {
        scene->setAmbientLight(new AmbientLight({ .intensity = 1.0f }));

        // Add the hand nodes.
        handModelLeft = std::make_unique<Model>(ModelCreateParams{
            .flipTextures = true,
            .gammaCorrected = true,
            .path = "models/quest-touch-plus-left.glb"
        });
        handNodes[0].setPosition({ 0.0065f, -0.008f, -0.04f });
        handNodes[0].setRotationEuler({ -16.0f, 0.0f, 0.0f });
        handNodes[0].setEntity(handModelLeft.get());

        handModelRight = std::make_unique<Model>(ModelCreateParams{
            .flipTextures = true,
            .gammaCorrected = true,
            .path = "models/quest-touch-plus-right.glb"
        });
        handNodes[1].setPosition({ -0.0065f, -0.008f, -0.04f });
        handNodes[1].setRotationEuler({ -16.0f, 0.0f, 0.0f });
        handNodes[1].setEntity(handModelRight.get());

        // Create video texture for color stream
        videoTextureColor = new VideoTexture({
            .width = videoSize.x,
            .height = videoSize.y,
            .internalFormat = GL_RGB8,
            .format = GL_RGB,
            .type = GL_UNSIGNED_BYTE,
            .wrapS = GL_CLAMP_TO_EDGE,
            .wrapT = GL_CLAMP_TO_EDGE,
            .minFilter = GL_LINEAR,
            .magFilter = GL_LINEAR
        }, videoURL);

        // Create BC4 depth texture
        videoTextureDepth = new BC4DepthVideoTexture({
            .width = videoSize.x / depthFactor,
            .height = videoSize.y / depthFactor,
            .internalFormat = GL_R32F,
            .format = GL_RED,
            .type = GL_FLOAT,
            .wrapS = GL_CLAMP_TO_EDGE,
            .wrapT = GL_CLAMP_TO_EDGE,
            .minFilter = GL_NEAREST,
            .magFilter = GL_NEAREST
        }, depthURL);

        // Create pose streamer
        remoteCamera.setFovyDegrees(remoteFOV);
        poseStreamer = std::make_unique<PoseStreamer>(&remoteCamera, poseURL);

        // Setup scene and mesh
        glm::uvec2 adjustedvideoSize = videoSize / surfelSize;
        unsigned int maxVertices = adjustedvideoSize.x * adjustedvideoSize.y;
        unsigned int numTriangles = (adjustedvideoSize.x-1) * (adjustedvideoSize.y-1) * 2;
        unsigned int maxIndices = numTriangles * 3;

        mesh = new Mesh({
            .maxVertices = maxVertices,
            .maxIndices = maxIndices,
            .material = new UnlitMaterial({ .baseColorTexture = videoTextureColor }),
            .usage = GL_DYNAMIC_DRAW
        });
        node.setEntity(mesh);
        node.frustumCulled = false;
        scene->addChildNode(&node);

        nodeWireframe.setEntity(mesh);
        nodeWireframe.frustumCulled = false;
        nodeWireframe.wireframe = true;
        nodeWireframe.visible = false;
        nodeWireframe.primitiveType = GL_LINES;
        nodeWireframe.overrideMaterial = new UnlitMaterial({ .baseColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) });
        scene->addChildNode(&nodeWireframe);

        // // Add a screen for the video
        // Cube* videoScreen = new Cube({
        //     .material = new UnlitMaterial({ .baseColorTexture = videoTextureColor }),
        // });
        // Node* screen = new Node(videoScreen);
        // Screen->setPosition({ 0.0f, 0.0f, -2.0f });
        // Screen->setScale({ 1.0f, 0.5f, 0.05f });
        // Screen->frustumCulled = false;
        // Scene->addChildNode(screen);

        genMeshFromBC4Shader = std::make_unique<ComputeShader>(ComputeShaderDataCreateParams{
            .computeCodeData = SHADER_COMMON_MESH_FROM_BC4_COMP,
            .computeCodeSize = SHADER_COMMON_MESH_FROM_BC4_COMP_len,
            .defines = {
                "#define THREADS_PER_LOCALGROUP " + std::to_string(GEN_MESH_THREADS_PER_LOCALGROUP)
            }
        });
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
            if (buzz[i] < 0.01f) {
                buzz[i] = 0.0f;
            }
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
        for (int i = 0; i < 2; i++) {
            handNodes[i].visible = handPoseState[i].isActive;

            if (clickState[i].isActive == XR_TRUE &&
                clickState[i].currentState == XR_FALSE &&
                clickState[i].changedSinceLastSync == XR_TRUE) {
                // XR_LOG("Click action triggered for hand: " << i);
                buzz[i] = 0.5f;

                nodeWireframe.visible = !nodeWireframe.visible;
            }

            if (thumbstickState[i].isActive == XR_TRUE && thumbstickState[i].changedSinceLastSync == XR_TRUE) {
                if (glm::abs(thumbstickState[i].currentState.x) > 0.2f || glm::abs(thumbstickState[i].currentState.y) > 0.2f) {
                    const glm::vec3 &forward = cameras->left.getForwardVector();
                    const glm::vec3 &right = cameras->left.getRightVector();
                    cameraPositionOffset += movementSpeed * forward * thumbstickState[i].currentState.y;
                    cameraPositionOffset += movementSpeed * right * thumbstickState[i].currentState.x;
                }
                // XR_LOG("Thumbstick action triggered for hand: " << i << " with value: " << thumbstickState[i].currentState.x << ", " << thumbstickState[i].currentState.y);
            }
        }
    }

    void OnRender(double now, double dt) override {
        // Update and send pose
        const glm::vec3& headPosition = (cameras->left.getPosition() + cameras->right.getPosition()) / 2.0f;
        const glm::quat& headRotation = glm::normalize(glm::slerp(
            cameras->left.getRotationQuat(),
            cameras->right.getRotationQuat(),
            0.5f
        ));
        remoteCamera.setPosition(headPosition);
        remoteCamera.setRotationQuat(headRotation);
        remoteCamera.updateViewMatrix();
        poseStreamer->sendPose();

        // Get latest video frames
        videoTextureColor->bind();
        poseIdColor = videoTextureColor->draw();

        // Get latest depth frames
        videoTextureDepth->bind();
        poseIdDepth = videoTextureDepth->draw(poseIdColor);
        spdlog::info("poseIdColor: {}, poseIdDepth: {}", poseIdColor, poseIdDepth);

        // Set shader uniforms
        genMeshFromBC4Shader->bind();
        {
            genMeshFromBC4Shader->setBool("unlinearizeDepth", true);
            genMeshFromBC4Shader->setVec2("depthMapSize", glm::vec2(videoTextureDepth->width, videoTextureDepth->height));
            genMeshFromBC4Shader->setInt("surfelSize", surfelSize);
        }
        {
            genMeshFromBC4Shader->setMat4("projection", remoteCamera.getProjectionMatrix());
            genMeshFromBC4Shader->setMat4("projectionInverse", glm::inverse(remoteCamera.getProjectionMatrix()));
            if (poseStreamer->getPose(poseIdColor, &currentColorFramePose, &elapsedTimeColor)) {
                genMeshFromBC4Shader->setMat4("viewColor", currentColorFramePose.mono.view);
            }
            if (poseStreamer->getPose(poseIdDepth, &currentDepthFramePose, &elapsedTimeDepth)) {
                genMeshFromBC4Shader->setMat4("viewInverseDepth", glm::inverse(currentDepthFramePose.mono.view));
            }

            genMeshFromBC4Shader->setFloat("near", remoteCamera.getNear());
            genMeshFromBC4Shader->setFloat("far", remoteCamera.getFar());
        }
        {
            genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 0, mesh->vertexBuffer);
            genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 1, mesh->indexBuffer);
            genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 2, videoTextureDepth->bc4CompressedBuffer);
        }

        // Dispatch compute shader to generate vertices and indices for both main and wireframe meshes
        genMeshFromBC4Shader->dispatch(
            ((videoTextureDepth->width / surfelSize) + GEN_MESH_THREADS_PER_LOCALGROUP - 1) / GEN_MESH_THREADS_PER_LOCALGROUP,
            ((videoTextureDepth->height / surfelSize) + GEN_MESH_THREADS_PER_LOCALGROUP - 1) / GEN_MESH_THREADS_PER_LOCALGROUP,
            1);
        genMeshFromBC4Shader->memoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                                            GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT);

        poseStreamer->removePosesLessThan(std::min(poseIdColor, poseIdDepth));

        // Render
        renderStats = graphicsAPI->drawObjects(*scene, *cameras);

        if (glm::abs(elapsedTimeColor) > 1e-5f) {
            XR_LOG("E2E Latency (RGB): " << elapsedTimeColor << "ms");
        }
        if (glm::abs(elapsedTimeDepth) > 1e-5f) {
            XR_LOG("E2E Latency (D): " << elapsedTimeDepth << "ms");
        }
    }

    void DestroyResources() override {
        delete videoTextureColor;
        delete videoTextureDepth;
        delete mesh;
    }

    VideoTexture* videoTextureColor;
    BC4DepthVideoTexture* videoTextureDepth;
    std::unique_ptr<PoseStreamer> poseStreamer;

    pose_id_t poseIdColor = -1, poseIdDepth = -1;
    // Get poses for the current frames
    double elapsedTimeColor, elapsedTimeDepth;
    Pose currentColorFramePose, currentDepthFramePose;

    PerspectiveCamera remoteCamera;

    Mesh* mesh;
    Node node, nodeWireframe;

    std::unique_ptr<ComputeShader> genMeshFromBC4Shader;

    RenderStats renderStats;

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

    std::unique_ptr<Model> handModelLeft;
    std::unique_ptr<Model> handModelRight;
};

#endif // MESHWARP_CLIENT_H
