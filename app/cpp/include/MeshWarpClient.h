#ifndef MESHWARP_CLIENT_H
#define MESHWARP_CLIENT_H

#include <OpenXRApp.h>

#include <Primatives/Mesh.h>
#include <Primatives/Cube.h>
#include <Primatives/Model.h>

#include <Materials/UnlitMaterial.h>
#include <Lights/AmbientLight.h>

#include <PoseStreamer.h>
#include <VideoTexture.h>

#include <shaders_common.h>

#define THREADS_PER_LOCALGROUP 16

class MeshWarpClient final : public OpenXRApp {
private:
    std::string serverIP = "192.168.4.105";
    std::string poseURL = serverIP + ":54321";
    std::string videoURL = "0.0.0.0:12345";

public:
    MeshWarpClient(GraphicsAPI_Type apiType) : OpenXRApp(apiType) {}
    ~MeshWarpClient() = default;

private:
    void CreateResources() override {
        // Create shader for mesh generation from BC4 compressed depth
        genMeshFromBC4Shader = new ComputeShader({
            .computeCodeData = SHADER_COMMON_GENMESHFROMBC4_COMP,
            .computeCodeSize = SHADER_COMMON_GENMESHFROMBC4_COMP_len,
            .defines = {
                "#define THREADS_PER_LOCALGROUP " + std::to_string(THREADS_PER_LOCALGROUP)
            }
        });

        // Create video texture for color data
        videoTexture = new VideoTexture({
            .width = 2048,
            .height = 1024,
            .internalFormat = GL_SRGB8,
            .format = GL_RGB,
            .type = GL_UNSIGNED_BYTE,
            .wrapS = GL_CLAMP_TO_EDGE,
            .wrapT = GL_CLAMP_TO_EDGE,
            .minFilter = GL_LINEAR,
            .magFilter = GL_LINEAR
        }, videoURL);

        // Load BC4 compressed depth data from file
        auto bc4Data = FileIO::loadBinaryFile("assets/depth.bc4");
        bc4Buffer = new Buffer(bc4Data.size(), GL_SHADER_STORAGE_BUFFER);
        bc4Buffer->setData(bc4Data.data(), bc4Data.size());

        // Initialize pose streamer
        poseStreamer = std::make_unique<PoseStreamer>(cameras.get(), poseURL);

        // Set up ambient lighting
        AmbientLight* ambientLight = new AmbientLight({
            .intensity = 0.05f
        });
        scene->setAmbientLight(ambientLight);

        // Create mesh for warped video
        glm::uvec2 adjustedWindowSize = windowSize / surfelSize;
        unsigned int maxVertices = adjustedWindowSize.x * adjustedWindowSize.y;
        unsigned int numTriangles = (adjustedWindowSize.x-1) * (adjustedWindowSize.y-1) * 2;
        unsigned int maxIndices = numTriangles * 3;

        warpedMesh = new Mesh({
            .vertices = std::vector<Vertex>(maxVertices),
            .indices = std::vector<unsigned int>(maxIndices),
            .material = new UnlitMaterial({ .baseColorTexture = videoTexture }),
            .usage = GL_DYNAMIC_DRAW
        });

        Node* meshNode = new Node(warpedMesh);
        meshNode->setPosition(glm::vec3(0.0f, 0.0f, -2.0f));
        meshNode->frustumCulled = false;
        scene->addChildNode(meshNode);

        // Add controller models
        Model* leftControllerMesh = new Model({
            .flipTextures = true,
            .IBL = 0,
            .path = "models/oculus-touch-controller-v3-left.glb"
        });
        m_handNodes[0].setEntity(leftControllerMesh);

        Model* rightControllerMesh = new Model({
            .flipTextures = true,
            .IBL = 0,
            .path = "models/oculus-touch-controller-v3-right.glb"
        });
        m_handNodes[1].setEntity(rightControllerMesh);
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
            OPENXR_CHECK(xrGetActionStateBoolean(m_session, &actionStateGetInfo, &m_clickState[i]), "Failed to get Boolean State of Click action.");
        }

        for (int i = 0; i < 2; i++) {
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
            OPENXR_CHECK(xrApplyHapticFeedback(m_session, &hapticActionInfo, (XrHapticBaseHeader*)&vibration), "Failed to apply haptic feedback.");
        }
    }

    void HandleInteractions() override {
        for (int i = 0; i < 2; i++) {
            m_handNodes[i].visible = m_handPoseState[i].isActive;

            if (m_clickState[i].isActive == XR_TRUE && m_clickState[i].currentState == XR_FALSE && m_clickState[i].changedSinceLastSync == XR_TRUE) {
                XR_LOG("Click action triggered for hand: " << i);
                m_buzz[i] = 0.5f;
                meshWarpEnabled = !meshWarpEnabled;
            }
        }
    }

    void OnRender() override {
        // Send pose to streamer
        poseStreamer->sendPose();

        // Render video frame
        videoTexture->bind();
        poseID = videoTexture->draw();
        videoTexture->unbind();

        if (!meshWarpEnabled) {
            return;
        }

        // Get pose data
        if (poseID != prevPoseID && poseStreamer->getPose(poseID, &currentFramePose, &elapsedTime)) {
            // Generate mesh using compute shader
            genMeshFromBC4Shader->bind();
            
            // Set uniforms
            genMeshFromBC4Shader->setVec2("screenSize", windowSize);
            genMeshFromBC4Shader->setVec2("depthMapSize", videoTexture->getSize());
            genMeshFromBC4Shader->setInt("surfelSize", surfelSize);
            
            genMeshFromBC4Shader->setMat4("projection", cameras->getProjectionMatrix());
            genMeshFromBC4Shader->setMat4("projectionInverse", glm::inverse(cameras->getProjectionMatrix()));
            genMeshFromBC4Shader->setMat4("viewColor", currentFramePose.mono.view);
            genMeshFromBC4Shader->setMat4("viewInverseDepth", glm::inverse(currentFramePose.mono.view));
            
            genMeshFromBC4Shader->setFloat("near", cameras->near);
            genMeshFromBC4Shader->setFloat("far", cameras->far);
            
            // Set buffers
            genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 0, warpedMesh->vertexBuffer);
            genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 1, warpedMesh->indexBuffer);
            genMeshFromBC4Shader->setBuffer(GL_SHADER_STORAGE_BUFFER, 2, bc4Buffer);
            
            // Dispatch compute shader
            glm::uvec2 adjustedSize = videoTexture->getSize() / surfelSize;
            genMeshFromBC4Shader->dispatch(
                (adjustedSize.x + THREADS_PER_LOCALGROUP - 1) / THREADS_PER_LOCALGROUP,
                (adjustedSize.y + THREADS_PER_LOCALGROUP - 1) / THREADS_PER_LOCALGROUP,
                1
            );
            
            genMeshFromBC4Shader->memoryBarrier(
                GL_SHADER_STORAGE_BARRIER_BIT |
                GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT |
                GL_ELEMENT_ARRAY_BARRIER_BIT
            );

            poseStreamer->removePosesLessThan(poseID);
            prevPoseID = poseID;
        }

        // Draw scene
        m_graphicsAPI->drawObjects(*scene.get(), *cameras.get(), GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        if (glm::abs(elapsedTime) > 1e-5f) {
            XR_LOG("E2E Latency: " << elapsedTime << "ms");
        }
    }

    void DestroyResources() override {
        delete videoTexture;
        delete genMeshFromBC4Shader;
        delete bc4Buffer;
        delete warpedMesh;
    }

private:
    // Shaders
    ComputeShader* genMeshFromBC4Shader;
    bool meshWarpEnabled = true;

    // Textures and buffers
    VideoTexture* videoTexture;
    Buffer* bc4Buffer;
    Mesh* warpedMesh;

    // Window size and mesh parameters
    glm::uvec2 windowSize{2048, 1024};
    unsigned int surfelSize = 1;

    // Pose streaming
    pose_id_t poseID = -1;
    pose_id_t prevPoseID = -1;
    std::unique_ptr<PoseStreamer> poseStreamer;
    Pose currentFramePose;
    double elapsedTime;

    // Actions
    XrAction m_clickAction;
    XrActionStateBoolean m_clickState[2] = {{XR_TYPE_ACTION_STATE_BOOLEAN}, {XR_TYPE_ACTION_STATE_BOOLEAN}};
    XrAction m_buzzAction;
    float m_buzz[2] = {0, 0};
};

#endif // MESHWARP_CLIENT_H