#ifndef OPENGLES_RENDERER_H
#define OPENGLES_RENDERER_H

#include <memory>

#include <Framebuffer.h>
#include <Primitives/FullScreenQuad.h>
#include <RenderTargets/RenderTargetBase.h>

#include <GraphicsAPI.h>

#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)

namespace quasar {

class MultiviewFramebuffer : public Framebuffer {
public:
    uint numAttachments = 0;

    MultiviewFramebuffer() {
        glGenFramebuffers(1, &ID);
    }
    ~MultiviewFramebuffer() {
        glDeleteFramebuffers(1, &ID);
    }

    void attachTextureMultiview(GLuint textureID, GLenum attachment, GLint mipLevel, GLint arrayLayer, GLsizei layerCount) {
        glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, attachment, textureID, mipLevel, arrayLayer, layerCount);
        numAttachments++;
    }
};

class MultiviewRenderTarget : public RenderTargetBase {
public:
    Texture colorTexture;
    Texture depthStencilTexture;

    MultiviewRenderTarget(const RenderTargetCreateParams& params)
        : RenderTargetBase(params)
        , colorTexture({
            .width = width,
            .height = height,
            .internalFormat = params.internalFormat,
            .format = params.format,
            .type = params.type,
            .wrapS = params.wrapS,
            .wrapT = params.wrapT,
            .minFilter = params.minFilter,
            .magFilter = params.magFilter,
            .multiSampled = params.multiSampled,
            .array = true,
            .arrayLayers = 2,
        })
        , depthStencilTexture({
            .width = width,
            .height = height,
            .internalFormat = GL_DEPTH24_STENCIL8,
            .format = GL_DEPTH_STENCIL,
            .type = GL_UNSIGNED_INT_24_8,
            .wrapS = GL_CLAMP_TO_EDGE,
            .wrapT = GL_CLAMP_TO_EDGE,
            .minFilter = GL_NEAREST,
            .magFilter = GL_NEAREST,
            .multiSampled = params.multiSampled,
            .numSamples = params.numSamples,
            .array = true,
            .arrayLayers = 2,
        })
    {
        framebufferMV.bind();
        framebufferMV.attachTextureMultiview(colorTexture, GL_COLOR_ATTACHMENT0, 0, 0, 2);
        framebufferMV.attachTextureMultiview(depthStencilTexture, GL_DEPTH_STENCIL_ATTACHMENT, 0, 0, 2);

        if (!framebufferMV.checkStatus()) {
            throw std::runtime_error("Framebuffer is not complete!");
        }

        framebufferMV.unbind();
    }
    ~MultiviewRenderTarget() = default;

    void resize(uint width, uint height) override {
        this->width = width;
        this->height = height;

        colorTexture.resize(width, height);
        depthStencilTexture.resize(width, height);
    }

    void bind() const override {
        framebufferMV.bind();
        scissor.apply();
        viewport.apply();
    }

    void unbind() const override {
        framebufferMV.unbind();
    }

private:
    MultiviewFramebuffer framebufferMV;
};

class OpenGLESRenderer : public GraphicsAPI {
public:
    OpenGLESRenderer(const Config& config, XrInstance xrInstance, XrSystemId systemId);
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

    virtual void setScreenShaderUniforms(const Shader& screenShader) override;

    virtual void beginRendering() override;
    virtual void endRendering() override;

    virtual RenderStats drawToScreen(const Shader& screenShader, const RenderTargetBase* overrideRenderTarget = nullptr) override;

private:
    virtual const std::vector<int64_t> GetSupportedColorSwapchainFormats() override;
    virtual const std::vector<int64_t> GetSupportedDepthSwapchainFormats() override;

    ksGpuWindow window{};

    PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR = nullptr;
    XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding{};

    std::unordered_map <XrSwapchain, std::pair<SwapchainType, std::vector<XrSwapchainImageOpenGLESKHR>>> swapchainImagesMap{};

    std::unordered_map<GLuint, ImageCreateInfo> images{};
    std::unordered_map<GLuint, ImageViewCreateInfo> imageViews{};
    std::unordered_map<GLuint, std::unique_ptr<MultiviewFramebuffer>> imageViewFramebuffers{};

    std::unique_ptr<FullScreenQuad> outputFsQuad;
    std::unique_ptr<MultiviewRenderTarget> outputRT;
    std::unique_ptr<MultiviewFramebuffer> displayFBO;
};
#endif

} // namespace quasar

#endif // OPENGLES_RENDERER_H
