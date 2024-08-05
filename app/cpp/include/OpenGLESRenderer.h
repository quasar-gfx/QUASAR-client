#ifndef OPENGLES_RENDERER_H
#define OPENGLES_RENDERER_H

#include <GraphicsAPI.h>

#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
class OpenGLESRenderer : public GraphicsAPI {
public:
    OpenGLESRenderer();
    OpenGLESRenderer(XrInstance m_xrInstance, XrSystemId systemId);
    ~OpenGLESRenderer();

    virtual void* CreateDesktopSwapchain(const SwapchainCreateInfo& swapchainCI) override;
    virtual void DestroyDesktopSwapchain(void*& swapchain) override;
    virtual void* GetDesktopSwapchainImage(void* swapchain, uint32_t index) override;
    virtual void AcquireDesktopSwapchanImage(void* swapchain, uint32_t& index) override;
    virtual void PresentDesktopSwapchainImage(void* swapchain, uint32_t index) override;

    // XR_DOCS_TAG_BEGIN_GetDepthFormat_OpenGL_ES
    virtual int64_t GetDepthFormat() override { return (int64_t)GL_DEPTH_COMPONENT32F; }
    // XR_DOCS_TAG_END_GetDepthFormat_OpenGL_ES
    virtual size_t AlignSizeForUniformBuffer(size_t size) override;

    virtual void* GetGraphicsBinding() override;
    virtual XrSwapchainImageBaseHeader* AllocateSwapchainImageData(XrSwapchain swapchain, SwapchainType type, uint32_t count) override;
    virtual void FreeSwapchainImageData(XrSwapchain swapchain) override {
        swapchainImagesMap[swapchain].second.clear();
        swapchainImagesMap.erase(swapchain);
    }
    virtual XrSwapchainImageBaseHeader* GetSwapchainImageData(XrSwapchain swapchain, uint32_t index) override { return (XrSwapchainImageBaseHeader*)&swapchainImagesMap[swapchain].second[index]; }
    // XR_DOCS_TAG_BEGIN_GetSwapchainImage_OpenGL_ES
    virtual void* GetSwapchainImage(XrSwapchain swapchain, uint32_t index) override { return (void*)(uint64_t)swapchainImagesMap[swapchain].second[index].image; }
    // XR_DOCS_TAG_END_GetSwapchainImage_OpenGL_ES

    virtual void* CreateImageView(const ImageViewCreateInfo& imageViewCI) override;
    virtual void DestroyImageView(void*& imageView) override;

    virtual void CreatePipeline(const PipelineCreateInfo& pipelineCI) override;

    virtual void BeginRendering() override;
    virtual void EndRendering() override;

    virtual void ClearColor(void* imageView, float r, float g, float b, float a) override;
    virtual void ClearDepth(void* imageView, float d) override;

    virtual void SetRenderAttachments(void** colorViews, size_t colorViewCount, void* depthStencilView, uint32_t width, uint32_t height) override;
    virtual void SetViewports(Viewport* viewports, size_t count) override;
    virtual void SetScissors(Rect2D* scissors, size_t count) override;

    virtual RenderStats DrawNode(Scene &scene, Camera cameras[], Node* node, const glm::mat4 &parentTransform,
                                 bool frustumCull = true, const Material* overrideMaterial = nullptr) override;

private:
    virtual const std::vector<int64_t> GetSupportedColorSwapchainFormats() override;
    virtual const std::vector<int64_t> GetSupportedDepthSwapchainFormats() override;

    ksGpuWindow window{};

    PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR = nullptr;
    XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding{};

    std::unordered_map <XrSwapchain, std::pair<SwapchainType, std::vector<XrSwapchainImageOpenGLESKHR>>> swapchainImagesMap{};

    std::unordered_map<GLuint, BufferCreateInfo> buffers{};
    std::unordered_map<GLuint, ImageCreateInfo> images{};
    std::unordered_map<GLuint, ImageViewCreateInfo> imageViews{};

    GLuint framebuffer;
    PipelineCreateInfo pipeline;
};
#endif

#endif // OPENGLES_RENDERER_H
