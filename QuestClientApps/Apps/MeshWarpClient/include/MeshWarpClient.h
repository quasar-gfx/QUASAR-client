#ifndef MESHWARP_CLIENT_H
#define MESHWARP_CLIENT_H

#include <OpenXRApp.h>

#include <Primitives/Mesh.h>
#include <Primitives/Cube.h>
#include <Primitives/Model.h>
#include <Materials/UnlitMaterial.h>

#include <Cameras/PerspectiveCamera.h>
#include <Utils/FileIO.h>

#include <VideoTexture.h>
#include <BC4DepthVideoTexture.h>
#include <PoseStreamer.h>

#include <shaders_common.h>

#define THREADS_PER_LOCALGROUP 16

using namespace quasar;

class MeshWarpClient final : public OpenXRApp {
private:
    std::string serverIP = "192.168.4.140";
    std::string poseURL = serverIP + ":54321";
    std::string videoURL = "0.0.0.0:12345";
    std::string depthURL = serverIP + ":65432";

    unsigned int surfelSize = 1;
    unsigned int depthFactor = 4;
    float fov = 120.0f;
    glm::uvec2 videoSize = glm::uvec2(1920, 1080);

    bool meshWarpEnabled = true;

public:
    MeshWarpClient(GraphicsAPI_Type apiType) : OpenXRApp(apiType), remoteCamera(videoSize.x, videoSize.y) {}
    ~MeshWarpClient() = default;

private:
    void CreateResources() override {
        scene->backgroundColor = glm::vec4(0.17f, 0.17f, 0.17f, 1.0f);

        AmbientLight* ambientLight = new AmbientLight({
            .intensity = 0.05f
        });
        scene->setAmbientLight(ambientLight);

        // Initialize video texture for color stream
        videoTextureColor = new VideoTexture({
            .width = videoSize.x,
            .height = videoSize.y,
            .internalFormat = GL_SRGB8,
            .format = GL_RGB,
            .type = GL_UNSIGNED_BYTE,
            .wrapS = GL_CLAMP_TO_EDGE,
            .wrapT = GL_CLAMP_TO_EDGE,
            .minFilter = GL_LINEAR,
            .magFilter = GL_LINEAR
        }, videoURL);

        // Initialize BC4 depth texture
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

        // remote camera
        remoteCamera.setFovyDegrees(fov);
        remoteCamera.updateViewMatrix();

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

        // Initialize pose streamer
        poseStreamer = new PoseStreamer(cameras.get(), poseURL);

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
        node = new Node(mesh);
        node->frustumCulled = false;
        scene->addChildNode(node);

        nodeWireframe = new Node(mesh);
        nodeWireframe->frustumCulled = false;
        nodeWireframe->wireframe = true;
        nodeWireframe->visible = false;
        nodeWireframe->overrideMaterial = new UnlitMaterial({ .baseColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) });
        scene->addChildNode(nodeWireframe);

        // // add a screen for the video.
        // Cube* videoScreen = new Cube({
        //     .material = new UnlitMaterial({ .baseColorTexture = videoTextureColor }),
        // });
        // Node* screen = new Node(videoScreen);
        // screen->setPosition(glm::vec3(0.0f, 0.0f, -2.0f));
        // screen->setScale(glm::vec3(1.0f, 0.5f, 0.05f));
        // screen->frustumCulled = false;
        // scene->addChildNode(screen);

        genMeshFromBC4Shader = new ComputeShader({
            .computeCodeData = SHADER_COMMON_MESHFROMBC4_COMP,
            .computeCodeSize = SHADER_COMMON_MESHFROMBC4_COMP_len,
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
        for (int i = 0; i < 2; i++) {
            m_handNodes[i].visible = m_handPoseState[i].isActive;

            if (m_clickState[i].isActive == XR_TRUE &&
                m_clickState[i].currentState == XR_FALSE &&
                m_clickState[i].changedSinceLastSync == XR_TRUE) {
                // XR_LOG("Click action triggered for hand: " << i);
                m_buzz[i] = 0.5f;

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
        // Update pose and stream it
        poseStreamer->sendPose();

        // Get latest video frames
        videoTextureColor->bind();
        poseIdColor = videoTextureColor->draw();
        videoTextureColor->unbind();

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
                ((videoTextureDepth->width / surfelSize) + THREADS_PER_LOCALGROUP - 1) / THREADS_PER_LOCALGROUP,
                ((videoTextureDepth->height / surfelSize) + THREADS_PER_LOCALGROUP - 1) / THREADS_PER_LOCALGROUP,
                1
            );
        genMeshFromBC4Shader->memoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                                            GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT);

        poseStreamer->removePosesLessThan(std::min(poseIdColor, poseIdDepth));

        // Render
        renderStats = m_graphicsAPI->drawObjects(*scene.get(), *cameras.get());

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
        delete node;
        delete genMeshFromBC4Shader;
    }

    VideoTexture* videoTextureColor;
    BC4DepthVideoTexture* videoTextureDepth;
    PoseStreamer* poseStreamer;

    pose_id_t poseIdColor = -1;
    pose_id_t poseIdDepth = -1;
    // Get poses for the current frames
    double elapsedTimeColor, elapsedTimeDepth;
    Pose currentColorFramePose, currentDepthFramePose;

    PerspectiveCamera remoteCamera;

    Mesh* mesh;
    Node* node;
    Node* nodeWireframe;

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
    float movementSpeed = 0.02f;
    // The haptic output action for grabbing cubes.
    XrAction m_buzzAction;
    // The current haptic output value for each controller.
    float m_buzz[2] = {0, 0};
};

#endif // MESHWARP_CLIENT_H
