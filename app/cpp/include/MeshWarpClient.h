#ifndef MESHWARP_CLIENT_H
#define MESHWARP_CLIENT_H

#include <OpenXRApp.h>

#include <Primatives/Mesh.h>
#include <Primatives/Cube.h>
#include <Materials/UnlitMaterial.h>

#include <Cameras/PerspectiveCamera.h>
#include <Utils/FileIO.h>

#include <VideoTexture.h>
#include <BC4DepthVideoTexture.h>
#include <PoseStreamer.h>

#include <shaders_common.h>

#define THREADS_PER_LOCALGROUP 16

class MeshWarpClient final : public OpenXRApp {
private:
    std::string serverIP = "192.168.4.140";
    std::string poseURL = serverIP + ":54321";
    std::string videoURL = "0.0.0.0:12345";
    std::string depthURL = serverIP + ":65432";

    unsigned int surfelSize = 1;
    unsigned int depthFactor = 8;
    float fov = 120.0f;
    glm::uvec2 windowSize = glm::uvec2(1920, 1080);

public:
    MeshWarpClient(GraphicsAPI_Type apiType) : OpenXRApp(apiType) {}
    ~MeshWarpClient() {
        cleanup();
    }

private:
    void CreateResources() override {
        scene->backgroundColor = glm::vec4(0.17f, 0.17f, 0.17f, 1.0f);

        // Initialize video texture for color stream
        videoTextureColor = new VideoTexture({
            .width = windowSize.x,
            .height = windowSize.y,
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
            .width = windowSize.x/depthFactor,
            .height = windowSize.y/depthFactor,
            .internalFormat = GL_R16F,
            .format = GL_RED,
            .type = GL_FLOAT,
            .wrapS = GL_CLAMP_TO_EDGE,
            .wrapT = GL_CLAMP_TO_EDGE,
            .minFilter = GL_NEAREST,
            .magFilter = GL_NEAREST
        }, depthURL);


        // remote camera
        remoteCamera = PerspectiveCamera(videoTextureColor->width, videoTextureColor->height);
        remoteCamera.setFovyDegrees(fov);
        remoteCamera.updateViewMatrix();

        // Initialize pose streamer
        poseStreamer = new PoseStreamer(cameras.get(), poseURL);

        // Setup scene & mesh
        glm::uvec2 adjustedWindowSize = windowSize / surfelSize;
        unsigned int maxVertices = adjustedWindowSize.x * adjustedWindowSize.y;
        unsigned int numTriangles = (adjustedWindowSize.x-1) * (adjustedWindowSize.y-1) * 2;
        unsigned int maxIndices = numTriangles * 3;

        mesh = new Mesh({
            .vertices = std::vector<Vertex>(maxVertices),
            .indices = std::vector<unsigned int>(maxIndices),
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

        // add a screen for the video.
        // Cube* videoScreen = new Cube({
        //     .material = new UnlitMaterial({ .baseColorTexture = colorTexture }),
        // });
        // Node* screen = new Node(videoScreen);
        // screen->setPosition(glm::vec3(0.0f, 0.0f, -2.0f));
        // screen->setScale(glm::vec3(1.0f, 0.5f, 0.05f));
        // screen->frustumCulled = false;
        // scene->addChildNode(screen);

        genMeshFromBC4Shader = new ComputeShader({
            .computeCodeData = SHADER_COMMON_GENMESHFROMBC4_COMP,
            .computeCodeSize = SHADER_COMMON_GENMESHFROMBC4_COMP_len,
            .defines = {
                "#define THREADS_PER_LOCALGROUP " + std::to_string(THREADS_PER_LOCALGROUP)
            }
        });

        AmbientLight* ambientLight = new AmbientLight({
            .intensity = 0.05f
        });
        scene->setAmbientLight(ambientLight);

    }

    void CreateActionSet() override {
        CreateAction(m_clickAction, "click-controller", XR_ACTION_TYPE_BOOLEAN_INPUT, {"/user/hand/left", "/user/hand/right"});
        CreateAction(m_buzzAction, "buzz", XR_ACTION_TYPE_VIBRATION_OUTPUT, {"/user/hand/left", "/user/hand/right"});
    }

    void SuggestBindings(std::map<std::string, std::vector<XrActionSuggestedBinding>>& bindings) override {
        bindings["/interaction_profiles/khr/simple_controller"].push_back({m_clickAction, CreateXrPath("/user/hand/left/input/select/click")});
        bindings["/interaction_profiles/khr/simple_controller"].push_back({m_clickAction, CreateXrPath("/user/hand/right/input/select/click")});
        bindings["/interaction_profiles/khr/simple_controller"].push_back({m_buzzAction, CreateXrPath("/user/hand/left/output/haptic")});
        bindings["/interaction_profiles/khr/simple_controller"].push_back({m_buzzAction, CreateXrPath("/user/hand/right/output/haptic")});

        bindings["/interaction_profiles/oculus/touch_controller"].push_back({m_clickAction, CreateXrPath("/user/hand/left/input/trigger/value")});
        bindings["/interaction_profiles/oculus/touch_controller"].push_back({m_clickAction, CreateXrPath("/user/hand/right/input/trigger/value")});
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
        }

        for (int i = 0; i < 2; i++) {
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
                XR_LOG("Click action triggered for hand: " << i);
                m_buzz[i] = 0.5f;
            }
        }
    }

    void OnRender() override {
        // Update pose and stream it
        poseStreamer->sendPose();

        // Get latest video frames
        videoTextureColor->bind();
        poseIdColor = videoTextureColor->draw();
        videoTextureColor->unbind();

        // Get latest depth frames
        videoTextureDepth->bind();
        poseIdDepth = videoTextureDepth->draw(poseIdColor);

        // set shader
        genMeshFromBC4Shader->bind();

        genMeshFromBC4Shader->setBool("unlinearizeDepth", true);
        genMeshFromBC4Shader->setVec2("screenSize", windowSize);
        genMeshFromBC4Shader->setVec2("depthMapSize", glm::vec2(videoTextureDepth->width, videoTextureDepth->height));
        genMeshFromBC4Shader->setInt("surfelSize", surfelSize);

        genMeshFromBC4Shader->setMat4("projection", remoteCamera.getProjectionMatrix());
        genMeshFromBC4Shader->setMat4("projectionInverse", glm::inverse(remoteCamera.getProjectionMatrix()));

        if (poseStreamer->getPose(poseIdColor, &currentColorFramePose, &elapsedTimeColor)) {
            genMeshFromBC4Shader->setMat4("viewColor", currentColorFramePose.mono.view);
        }
        if (poseStreamer->getPose(poseIdDepth, &currentDepthFramePose, &elapsedTimeDepth)) {
            genMeshFromBC4Shader->setMat4("viewInverseDepth", glm::inverse(currentDepthFramePose.mono.view));
        }

        genMeshFromBC4Shader->setFloat("near", remoteCamera.near);
        genMeshFromBC4Shader->setFloat("far", remoteCamera.far);

        genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 0, mesh->vertexBuffer);
        genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 1, mesh->indexBuffer);
        genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 2, videoTextureDepth->bc4CompressedBuffer);

        // dispatch compute shader to generate vertices and indices for both main and wireframe meshes
        genMeshFromBC4Shader->dispatch(((videoTextureDepth->width / surfelSize) + THREADS_PER_LOCALGROUP - 1) / THREADS_PER_LOCALGROUP,
                                       ((videoTextureDepth->height / surfelSize) + THREADS_PER_LOCALGROUP - 1) / THREADS_PER_LOCALGROUP, 1);
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

    void cleanup() {
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

    Texture* colorTexture;

    Mesh* mesh;
    Node* node;
    Node* nodeWireframe;

    ComputeShader* genMeshFromBC4Shader;

    RenderStats renderStats;

    bool meshWarpEnabled = true;

    XrAction m_clickAction{XR_NULL_HANDLE};
    XrAction m_buzzAction{XR_NULL_HANDLE};
    XrActionStateBoolean m_clickState[2] = {{XR_TYPE_ACTION_STATE_BOOLEAN}, {XR_TYPE_ACTION_STATE_BOOLEAN}};
    float m_buzz[2] = {0, 0};
};

#endif // MESHWARP_CLIENT_H
