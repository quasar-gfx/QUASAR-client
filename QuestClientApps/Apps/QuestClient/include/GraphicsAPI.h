// Copyright 2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

// OpenXR Tutorial for Khronos Group

#pragma once
#include <Utils/HelperFunctions.h>

#include <android_native_app_glue.h>
#define XR_USE_PLATFORM_ANDROID

#define XR_USE_GRAPHICS_API_OPENGL_ES

#include <gfxwrapper_opengl.h>

// OpenXR Helper
#include <Utils/OpenXRHelper.h>

#include <OpenGLAppConfig.h>
#include <Renderers/OpenGLRenderer.h>

enum GraphicsAPI_Type : uint8_t {
    UNKNOWN,
    D3D11,
    D3D12,
    OPENGL,
    OPENGL_ES,
    VULKAN
};

bool CheckGraphicsAPI_TypeIsValidForPlatform(GraphicsAPI_Type type);

const char* GetGraphicsAPIInstanceExtensionString(GraphicsAPI_Type type);

class GraphicsAPI : public OpenGLRenderer {
public:
    enum class SwapchainType : uint8_t {
        COLOR,
        DEPTH
    };

    struct SwapchainCreateInfo {
        uint32_t width;
        uint32_t height;
        uint32_t count;
        void* windowHandle;
        int64_t format;
        bool vsync;
    };

    struct ImageCreateInfo {
        uint32_t dimension;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t mipLevels;
        uint32_t arrayLayers;
        uint32_t sampleCount;
        int64_t format;
        bool cubemap;
        bool colorAttachment;
        bool depthAttachment;
        bool sampled;
    };

    struct ImageViewCreateInfo {
        void* image;
        enum class Type : uint8_t {
            RTV,
            DSV,
            SRV,
            UAV
        } type;
        enum class View : uint8_t {
            TYPE_1D,
            TYPE_2D,
            TYPE_3D,
            TYPE_CUBE,
            TYPE_1D_ARRAY,
            TYPE_2D_ARRAY,
            TYPE_CUBE_ARRAY,
        } view;
        int64_t format;
        enum class Aspect : uint8_t {
            COLOR_BIT = 0x01,
            DEPTH_BIT = 0x02,
            STENCIL_BIT = 0x04
        } aspect;
        uint32_t baseMipLevel;
        uint32_t levelCount;
        uint32_t baseArrayLayer;
        uint32_t layerCount;
    };

    struct Viewport {
        float x;
        float y;
        float width;
        float height;
        float minDepth;
        float maxDepth;
    };
    struct Offset2D {
        int32_t x;
        int32_t y;
    };
    struct Extent2D {
        uint32_t width;
        uint32_t height;
    };
    struct Rect2D {
        Offset2D offset;
        Extent2D extent;
    };

public:
    GraphicsAPI(const Config &config) : OpenGLRenderer(config) {}
    virtual ~GraphicsAPI() = default;

    int64_t SelectColorSwapchainFormat(const std::vector<int64_t>& formats);
    int64_t SelectDepthSwapchainFormat(const std::vector<int64_t>& formats);

    virtual void* GetGraphicsBinding() = 0;
    virtual XrSwapchainImageBaseHeader* AllocateSwapchainImageData(XrSwapchain swapchain, SwapchainType type, uint32_t count) = 0;
    virtual void FreeSwapchainImageData(XrSwapchain swapchain) = 0;
    virtual XrSwapchainImageBaseHeader* GetSwapchainImageData(XrSwapchain swapchain, uint32_t index) = 0;
    virtual void* GetSwapchainImage(XrSwapchain swapchain, uint32_t index) = 0;

    virtual void* CreateImageView(const ImageViewCreateInfo& imageViewCI) = 0;
    virtual void DestroyImageView(void*& imageView) = 0;

    virtual void SetRenderAttachments(void** colorViews, size_t colorViewCount, void* depthStencilView, uint32_t width, uint32_t height) = 0;

    // OpenGLRenderer-related functions:
    virtual void setScreenShaderUniforms(const Shader &screenShader) override = 0;

    virtual void beginRendering() override = 0;
    virtual void endRendering() override = 0;

    virtual RenderStats drawObjects(const Scene &scene, const Camera &camera, uint32_t clearMask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT) override = 0;
    virtual RenderStats drawToScreen(const Shader &screenShader, const RenderTargetBase* overrideRenderTarget = nullptr) override = 0;

protected:
    virtual const std::vector<int64_t> GetSupportedColorSwapchainFormats() = 0;
    virtual const std::vector<int64_t> GetSupportedDepthSwapchainFormats() = 0;
    bool debugAPI = false;
};
