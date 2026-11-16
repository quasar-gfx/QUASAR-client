#ifndef OPENGLES_RENDERER_H
#define OPENGLES_RENDERER_H

#include <memory>
#include <unordered_map>

#include <Framebuffer.h>
#include <Primitives/FullScreenQuad.h>
#include <RenderTargets/RenderTargetBase.h>

#include <Utils/HelperFunctions.h>

#include <android_native_app_glue.h>
#define XR_USE_PLATFORM_ANDROID

#define XR_USE_GRAPHICS_API_OPENGL_ES

#include <gfxwrapper_opengl.h>

// OpenXR Helper
#include <Utils/OpenXRHelper.h>

#include <OpenGLAppConfig.h>
#include <Renderers/OpenGLRenderer.h>

namespace quasar {

struct ImageViewCreateInfo {
    void* image;
    enum class Aspect : uint8_t {
        COLOR_BIT = 0x01,
        DEPTH_BIT = 0x02,
    } aspect;
    uint32_t layerCount;
};

class MultiviewFramebuffer : public Framebuffer {
public:
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

class OpenGLESRenderer : public OpenGLRenderer {
public:
    OpenGLESRenderer(const Config& config, XrInstance xrInstance, XrSystemId systemId);
    ~OpenGLESRenderer();

    int64_t SelectColorSwapchainFormat(const std::vector<int64_t>& formats);
    int64_t SelectDepthSwapchainFormat(const std::vector<int64_t>& formats);

    void* GetGraphicsBinding();
    XrSwapchainImageBaseHeader* AllocateSwapchainImageData(XrSwapchain swapchain, uint32_t count);
    void FreeSwapchainImageData(XrSwapchain swapchain) {
        swapchainImagesMap.erase(swapchain);
    }
    void* GetSwapchainImage(XrSwapchain swapchain, uint32_t index) { return (void*)(uint64_t)swapchainImagesMap[swapchain][index].image; }

    void* CreateImageView(const ImageViewCreateInfo& imageViewCI);
    void DestroyImageView(void*& imageView);

    void SetRenderAttachments(void* colorView, void* depthStencilView);

    void setScreenShaderUniforms(const Shader& screenShader) override;

    void beginRendering() override;
    void endRendering() override;

    RenderStats drawToScreen(const Shader& screenShader, const RenderTargetBase* overrideRenderTarget = nullptr) override;

private:
    const std::vector<int64_t> GetSupportedColorSwapchainFormats();
    const std::vector<int64_t> GetSupportedDepthSwapchainFormats();

    ksGpuWindow window{};

    PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR = nullptr;
    XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding{};

    std::unordered_map<XrSwapchain, std::vector<XrSwapchainImageOpenGLESKHR>> swapchainImagesMap{};

    std::unordered_map<GLuint, ImageViewCreateInfo> imageViews{};
    std::unordered_map<GLuint, std::shared_ptr<MultiviewFramebuffer>> imageViewFramebuffers{};

    std::shared_ptr<MultiviewRenderTarget> outputRT;
    std::shared_ptr<MultiviewFramebuffer> displayFBO;
};
} // namespace quasar

#endif // OPENGLES_RENDERER_H
