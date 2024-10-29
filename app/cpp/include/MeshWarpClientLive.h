#ifndef MESHWARPLIVE_CLIENT_H
#define MESHWARPLIVE_CLIENT_H

#include <OpenXRApp.h>
#include <Primatives/Mesh.h>
#include <Primatives/Cube.h>
#include <Materials/UnlitMaterial.h>
#include <Shaders/ToneMapShader.h>
#include <shaders_common.h>
#include <Buffer.h>
#include <Utils/FileIO.h>
#include <Renderers/OpenGLRenderer.h>
#include <Cameras/PerspectiveCamera.h>
#include <stb_image.h>

#include <VideoTexture.h>
#include <BC4DepthVideoTexture.h>
#include <PoseStreamer.h>

#define THREADS_PER_LOCALGROUP 16

class MeshWarpClientLive final : public OpenXRApp {
private:
    std::string serverIP = "192.168.4.105";
    std::string poseURL = serverIP + ":54321";
    std::string videoURL = "0.0.0.0:12345";
    std::string depthURL = "0.0.0.0:65432";

public:
    MeshWarpClientLive(GraphicsAPI_Type apiType) 
        : OpenXRApp(apiType)
        , windowSize(2048, 1024)
        , renderer(config)
        , remoteCamera(1024, 1024){}
    ~MeshWarpClientLive() {
        cleanup();
    }

private:
    void CreateResources() override {
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
            .width = windowSize.x, // depthFactor?
            .height = windowSize.y,
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
        remoteCamera.updateViewMatrix();

        // Initialize pose streamer
        poseStreamer = new PoseStreamer(&remoteCamera, poseURL);

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
                meshWarpEnabled = !meshWarpEnabled;
            }
        }
    }

    void OnRender() override {
        if (!meshWarpEnabled) {
            return;
        }

        // Get latest video frames
        videoTextureColor->bind();
        poseIdColor = videoTextureColor->draw();
        videoTextureColor->unbind();
        
        // Get latest depth frames
        videoTextureDepth->bind();
        poseIdDepth = videoTextureDepth->draw(poseIdColor);

        // Update pose and stream it
        poseStreamer->sendPose();
        

        
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
        genMeshFromBC4Shader.dispatch((videoTextureDepth.width + THREADS_PER_LOCALGROUP - 1) / THREADS_PER_LOCALGROUP,
                                      (videoTextureDepth.height + THREADS_PER_LOCALGROUP - 1) / THREADS_PER_LOCALGROUP, 1);
        genMeshFromBC4Shader.memoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                                           GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT);

        poseStreamer.removePosesLessThan(std::min(poseIdColor, poseIdDepth));

        // Set render state
        node.primativeType = renderState == RenderState::POINTCLOUD ? GL_POINTS : GL_TRIANGLES;
        nodeWireframe.visible = renderState == RenderState::WIREFRAME;

        // Render
        //renderer.pipeline.rasterState.cullFaceEnabled = false;
        renderStats = renderer.drawObjects(*scene, *cameras.get());
        //renderer.pipeline.rasterState.cullFaceEnabled = true;

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
    Buffer<BC4DepthVideoTexture::Block>* bc4BufferData;
    Mesh* mesh;
    Node* node;
    ComputeShader* genMeshFromBC4Shader;


    unsigned int surfelSize = 1;
    glm::uvec2 windowSize;
    OpenGLRenderer renderer;
    RenderStats renderStats;


    bool meshWarpEnabled = true;

    XrAction m_clickAction{XR_NULL_HANDLE};
    XrAction m_buzzAction{XR_NULL_HANDLE};
    XrActionStateBoolean m_clickState[2] = {{XR_TYPE_ACTION_STATE_BOOLEAN}, {XR_TYPE_ACTION_STATE_BOOLEAN}};
    float m_buzz[2] = {0, 0};
};

#endif // MESHWARPLIVE_CLIENT_H