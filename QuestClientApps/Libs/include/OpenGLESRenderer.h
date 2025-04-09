#ifndef OPENGLES_RENDERER_H
#define OPENGLES_RENDERER_H

#include <GraphicsAPI.h>

#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)

namespace quasar {

class OpenGLESRenderer : public GraphicsAPI {
public:
    OpenGLESRenderer(const Config &config, XrInstance m_xrInstance, XrSystemId systemId);
    ~OpenGLESRenderer();

    virtual void* GetGraphicsBinding() override;
    virtual XrSwapchainImageBaseHeader* AllocateSwapchainImageData(XrSwapchain swapchain, SwapchainType type, uint32_t count) override;
    virtual void FreeSwapchainImageData(XrSwapchain swapchain) override {
        swapchainImagesMap[swapchain].second.clear();
        swapchainImagesMap.erase(swapchain);
    }
    virtual XrSwapchainImageBaseHeader* GetSwapchainImageData(XrSwapchain swapchain, uint32_t index) override { return (XrSwapchainImageBaseHeader*)&swapchainImagesMap[swapchain].second[index]; }
    virtual void* GetSwapchainImage(XrSwapchain swapchain, uint32_t index) override { return (void*)(uint64_t)swapchainImagesMap[swapchain].second[index].image; }

    virtual void* CreateImageView(const ImageViewCreateInfo& imageViewCI) override;
    virtual void DestroyImageView(void*& imageView) override;

    virtual void SetRenderAttachments(void** colorViews, size_t colorViewCount, void* depthStencilView, uint32_t width, uint32_t height) override;

    // OpenGLRenderer-related functions:
    virtual void setScreenShaderUniforms(const Shader &screenShader) override;

    virtual void beginRendering() override;
    virtual void endRendering() override;

    virtual RenderStats drawObjects(const Scene &scene, const Camera &camera,
                                    uint32_t clearMask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT) override;
    virtual RenderStats drawToScreen(const Shader &screenShader, const RenderTargetBase* overrideRenderTarget = nullptr) override;

private:
    virtual const std::vector<int64_t> GetSupportedColorSwapchainFormats() override;
    virtual const std::vector<int64_t> GetSupportedDepthSwapchainFormats() override;

    ksGpuWindow window{};

    PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR = nullptr;
    XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding{};

    std::unordered_map <XrSwapchain, std::pair<SwapchainType, std::vector<XrSwapchainImageOpenGLESKHR>>> swapchainImagesMap{};

    std::unordered_map<GLuint, ImageCreateInfo> images{};
    std::unordered_map<GLuint, ImageViewCreateInfo> imageViews{};

    GLuint framebuffer;
};
#endif

} // namespace quasar

#endif // OPENGLES_RENDERER_H
