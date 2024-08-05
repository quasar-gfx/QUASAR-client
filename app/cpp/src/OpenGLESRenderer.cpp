// Copyright 2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

// OpenXR Tutorial for Khronos Group

#include <OpenGLESRenderer.h>

#include <Shaders/Shader.h>
#include <Primatives/Mesh.h>

#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#if defined(OS_WINDOWS)
PROC GetExtension(const char *functionName) { return wglGetProcAddress(functionName); }
#elif defined(OS_APPLE)
void (*GetExtension(const char *functionName))() { return NULL; }
#elif defined(OS_LINUX_XCB) || defined(OS_LINUX_XLIB) || defined(OS_LINUX_XCB_GLX)
void (*GetExtension(const char *functionName))() { return glXGetProcAddress((const GLubyte *)functionName); }
#elif defined(OS_ANDROID) || defined(OS_LINUX_WAYLAND)
void (*GetExtension(const char *functionName))() { return eglGetProcAddress(functionName); }
#endif

#pragma region PiplineHelpers

GLenum GetGLTextureTarget(const GraphicsAPI::ImageCreateInfo &imageCI) {
    GLenum target = 0;
    if (imageCI.dimension == 1) {
        if (imageCI.arrayLayers > 1) {
            target = GL_TEXTURE_1D_ARRAY;
        } else {
            target = GL_TEXTURE_1D;
        }
    } else if (imageCI.dimension == 2) {
        if (imageCI.cubemap) {
            if (imageCI.arrayLayers > 6) {
                target = GL_TEXTURE_CUBE_MAP_ARRAY;
            } else {
                target = GL_TEXTURE_CUBE_MAP;
            }
        } else {
            if (imageCI.sampleCount > 1) {
                if (imageCI.arrayLayers > 1) {
                    target = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
                } else {
                    target = GL_TEXTURE_2D_MULTISAMPLE;
                }
            } else {
                if (imageCI.arrayLayers > 1) {
                    target = GL_TEXTURE_2D_ARRAY;
                } else {
                    target = GL_TEXTURE_2D;
                }
            }
        }
    } else if (imageCI.dimension == 3) {
        target = GL_TEXTURE_3D;
    } else {
        DEBUG_BREAK;
        std::cout << "ERROR: OPENGL: Unknown Dimension for GetGLTextureTarget(): " << imageCI.dimension << std::endl;
    }
    return target;
}

GLint ToGLFilter(GraphicsAPI::SamplerCreateInfo::Filter filter) {
    switch (filter) {
    case GraphicsAPI::SamplerCreateInfo::Filter::NEAREST:
        return GL_NEAREST;
    case GraphicsAPI::SamplerCreateInfo::Filter::LINEAR:
        return GL_LINEAR;
    default:
        return 0;
    }
};
GLint ToGLFilterMipmap(GraphicsAPI::SamplerCreateInfo::Filter filter, GraphicsAPI::SamplerCreateInfo::MipmapMode mipmapMode) {
    switch (filter) {
    case GraphicsAPI::SamplerCreateInfo::Filter::NEAREST: {
        if (mipmapMode == GraphicsAPI::SamplerCreateInfo::MipmapMode::NEAREST)
            return GL_NEAREST_MIPMAP_LINEAR;
        else if (mipmapMode == GraphicsAPI::SamplerCreateInfo::MipmapMode::LINEAR)
            return GL_NEAREST_MIPMAP_NEAREST;
        else
            return GL_NEAREST;
    }
    case GraphicsAPI::SamplerCreateInfo::Filter::LINEAR: {
        if (mipmapMode == GraphicsAPI::SamplerCreateInfo::MipmapMode::NEAREST)
            return GL_LINEAR_MIPMAP_LINEAR;
        else if (mipmapMode == GraphicsAPI::SamplerCreateInfo::MipmapMode::LINEAR)
            return GL_LINEAR_MIPMAP_NEAREST;
        else
            return GL_LINEAR;
    }
    default:
        return 0;
    }
};
GLint ToGLAddressMode(GraphicsAPI::SamplerCreateInfo::AddressMode mode) {
    switch (mode) {
    case GraphicsAPI::SamplerCreateInfo::AddressMode::REPEAT:
        return GL_REPEAT;
    case GraphicsAPI::SamplerCreateInfo::AddressMode::MIRRORED_REPEAT:
        return GL_MIRRORED_REPEAT;
    case GraphicsAPI::SamplerCreateInfo::AddressMode::CLAMP_TO_EDGE:
        return GL_CLAMP_TO_EDGE;
    case GraphicsAPI::SamplerCreateInfo::AddressMode::CLAMP_TO_BORDER:
        return GL_CLAMP_TO_BORDER;
    case GraphicsAPI::SamplerCreateInfo::AddressMode::MIRROR_CLAMP_TO_EDGE:
        return 0; // GL_MIRROR_CLAMP_TO_EDGE // None for ES
    default:
        return 0;
    }
};

inline GLenum ToGLTopology(GraphicsAPI::PrimitiveTopology topology) {
    switch (topology) {
    case GraphicsAPI::PrimitiveTopology::POINT_LIST:
        return GL_POINTS;
    case GraphicsAPI::PrimitiveTopology::LINE_LIST:
        return GL_LINES;
    case GraphicsAPI::PrimitiveTopology::LINE_STRIP:
        return GL_LINE_STRIP;
    case GraphicsAPI::PrimitiveTopology::TRIANGLE_LIST:
        return GL_TRIANGLES;
    case GraphicsAPI::PrimitiveTopology::TRIANGLE_STRIP:
        return GL_TRIANGLE_STRIP;
    case GraphicsAPI::PrimitiveTopology::TRIANGLE_FAN:
        return GL_TRIANGLE_FAN;
    default:
        return 0;
    }
};
inline GLenum ToGLCullMode(GraphicsAPI::CullMode cullMode) {
    switch (cullMode) {
    case GraphicsAPI::CullMode::NONE:
        return GL_BACK;
    case GraphicsAPI::CullMode::FRONT:
        return GL_FRONT;
    case GraphicsAPI::CullMode::BACK:
        return GL_BACK;
    case GraphicsAPI::CullMode::FRONT_AND_BACK:
        return GL_FRONT_AND_BACK;
    default:
        return 0;
    }
}
inline GLenum ToGLCompareOp(GraphicsAPI::CompareOp op) {
    switch (op) {
    case GraphicsAPI::CompareOp::NEVER:
        return GL_NEVER;
    case GraphicsAPI::CompareOp::LESS:
        return GL_LESS;
    case GraphicsAPI::CompareOp::EQUAL:
        return GL_EQUAL;
    case GraphicsAPI::CompareOp::LESS_OR_EQUAL:
        return GL_LEQUAL;
    case GraphicsAPI::CompareOp::GREATER:
        return GL_GREATER;
    case GraphicsAPI::CompareOp::NOT_EQUAL:
        return GL_NOTEQUAL;
    case GraphicsAPI::CompareOp::GREATER_OR_EQUAL:
        return GL_GEQUAL;
    case GraphicsAPI::CompareOp::ALWAYS:
        return GL_ALWAYS;
    default:
        return 0;
    }
};
inline GLenum ToGLStencilCompareOp(GraphicsAPI::StencilOp op) {
    switch (op) {
    case GraphicsAPI::StencilOp::KEEP:
        return GL_KEEP;
    case GraphicsAPI::StencilOp::ZERO:
        return GL_ZERO;
    case GraphicsAPI::StencilOp::REPLACE:
        return GL_REPLACE;
    case GraphicsAPI::StencilOp::INCREMENT_AND_CLAMP:
        return GL_INCR;
    case GraphicsAPI::StencilOp::DECREMENT_AND_CLAMP:
        return GL_DECR;
    case GraphicsAPI::StencilOp::INVERT:
        return GL_INVERT;
    case GraphicsAPI::StencilOp::INCREMENT_AND_WRAP:
        return GL_INCR_WRAP;
    case GraphicsAPI::StencilOp::DECREMENT_AND_WRAP:
        return GL_DECR_WRAP;
    default:
        return 0;
    }
};
inline GLenum ToGLBlendFactor(GraphicsAPI::BlendFactor factor) {
    switch (factor) {
    case GraphicsAPI::BlendFactor::ZERO:
        return GL_ZERO;
    case GraphicsAPI::BlendFactor::ONE:
        return GL_ONE;
    case GraphicsAPI::BlendFactor::SRC_COLOR:
        return GL_SRC_COLOR;
    case GraphicsAPI::BlendFactor::ONE_MINUS_SRC_COLOR:
        return GL_ONE_MINUS_SRC_COLOR;
    case GraphicsAPI::BlendFactor::DST_COLOR:
        return GL_DST_COLOR;
    case GraphicsAPI::BlendFactor::ONE_MINUS_DST_COLOR:
        return GL_ONE_MINUS_DST_COLOR;
    case GraphicsAPI::BlendFactor::SRC_ALPHA:
        return GL_SRC_ALPHA;
    case GraphicsAPI::BlendFactor::ONE_MINUS_SRC_ALPHA:
        return GL_ONE_MINUS_SRC_ALPHA;
    case GraphicsAPI::BlendFactor::DST_ALPHA:
        return GL_DST_ALPHA;
    case GraphicsAPI::BlendFactor::ONE_MINUS_DST_ALPHA:
        return GL_ONE_MINUS_DST_ALPHA;
    default:
        return 0;
    }
};
inline GLenum ToGLBlendOp(GraphicsAPI::BlendOp op) {
    switch (op) {
    case GraphicsAPI::BlendOp::ADD:
        return GL_FUNC_ADD;
    case GraphicsAPI::BlendOp::SUBTRACT:
        return GL_FUNC_SUBTRACT;
    case GraphicsAPI::BlendOp::REVERSE_SUBTRACT:
        return GL_FUNC_REVERSE_SUBTRACT;
    case GraphicsAPI::BlendOp::MIN:
        return GL_MIN;
    case GraphicsAPI::BlendOp::MAX:
        return GL_MAX;
    default:
        return 0;
    }
};
#pragma endregion

void GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam) {
    std::cout << "OpenGL Debug message (" << id << "): " << message << std::endl;

    switch (source) {
    case GL_DEBUG_SOURCE_API:
        std::cout << "Source: API";
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        std::cout << "Source: Window System";
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        std::cout << "Source: Shader Compiler";
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        std::cout << "Source: Third Party";
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        std::cout << "Source: Application";
        break;
    case GL_DEBUG_SOURCE_OTHER:
        std::cout << "Source: Other";
        break;
    }
    std::cout << std::endl;

    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        std::cout << "Type: Error";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        std::cout << "Type: Deprecated Behaviour";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        std::cout << "Type: Undefined Behaviour";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        std::cout << "Type: Portability";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        std::cout << "Type: Performance";
        break;
    case GL_DEBUG_TYPE_MARKER:
        std::cout << "Type: Marker";
        break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
        std::cout << "Type: Push Group";
        break;
    case GL_DEBUG_TYPE_POP_GROUP:
        std::cout << "Type: Pop Group";
        break;
    case GL_DEBUG_TYPE_OTHER:
        std::cout << "Type: Other";
        break;
    }
    std::cout << std::endl;

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        std::cout << "Severity: high";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        std::cout << "Severity: medium";
        break;
    case GL_DEBUG_SEVERITY_LOW:
        std::cout << "Severity: low";
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        std::cout << "Severity: notification";
        break;
    }
    std::cout << std::endl;
    std::cout << std::endl;

    if (type == GL_DEBUG_TYPE_ERROR)
        DEBUG_BREAK;
}

OpenGLESRenderer::OpenGLESRenderer() {
    // https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/f122f9f1fc729e2dc82e12c3ce73efa875182854/src/tests/hello_xr/graphicsplugin_opengles.cpp#L101-L119
    // Initialize the gl extensions. Note we have to open a window.
    ksDriverInstance driverInstance{};
    ksGpuQueueInfo queueInfo{};
    ksGpuSurfaceColorFormat colorFormat{KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8};
    ksGpuSurfaceDepthFormat depthFormat{KS_GPU_SURFACE_DEPTH_FORMAT_D24};
    ksGpuSampleCount sampleCount{KS_GPU_SAMPLE_COUNT_1};
    if (!ksGpuWindow_Create(&window, &driverInstance, &queueInfo, 0, colorFormat, depthFormat, sampleCount, 640, 480, false)) {
        std::cout << "ERROR: OPENGL ES: Failed to create Context." << std::endl;
    }

    GLint glMajorVersion = 0;
    GLint glMinorVersion = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &glMajorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &glMinorVersion);

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(GLDebugCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_FALSE);
    glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, nullptr, GL_TRUE);
}

// XR_DOCS_TAG_BEGIN_OpenGLESRenderer
OpenGLESRenderer::OpenGLESRenderer(XrInstance m_xrInstance, XrSystemId systemId) {
    OPENXR_CHECK(xrGetInstanceProcAddr(m_xrInstance, "xrGetOpenGLESGraphicsRequirementsKHR", (PFN_xrVoidFunction *)&xrGetOpenGLESGraphicsRequirementsKHR), "Failed to get InstanceProcAddr for xrGetOpenGLESGraphicsRequirementsKHR.");
    XrGraphicsRequirementsOpenGLESKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
    OPENXR_CHECK(xrGetOpenGLESGraphicsRequirementsKHR(m_xrInstance, systemId, &graphicsRequirements), "Failed to get Graphics Requirements for OpenGLES.");

    // https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/f122f9f1fc729e2dc82e12c3ce73efa875182854/src/tests/hello_xr/graphicsplugin_opengles.cpp#L101-L119
    // Initialize the gl extensions. Note we have to open a window.
    ksDriverInstance driverInstance{};
    ksGpuQueueInfo queueInfo{};
    ksGpuSurfaceColorFormat colorFormat{KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8};
    ksGpuSurfaceDepthFormat depthFormat{KS_GPU_SURFACE_DEPTH_FORMAT_D24};
    ksGpuSampleCount sampleCount{KS_GPU_SAMPLE_COUNT_1};
    if (!ksGpuWindow_Create(&window, &driverInstance, &queueInfo, 0, colorFormat, depthFormat, sampleCount, 640, 480, false)) {
        std::cout << "ERROR: OPENGL ES: Failed to create Context." << std::endl;
    }

    GLint glMajorVersion = 0;
    GLint glMinorVersion = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &glMajorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &glMinorVersion);

    const XrVersion glApiVersion = XR_MAKE_VERSION(glMajorVersion, glMinorVersion, 0);
    if (graphicsRequirements.minApiVersionSupported > glApiVersion) {
        int requiredMajorVersion = XR_VERSION_MAJOR(graphicsRequirements.minApiVersionSupported);
        int requiredMinorVersion = XR_VERSION_MINOR(graphicsRequirements.minApiVersionSupported);
        std::cerr << "ERROR: OPENGL ES: The created OpenGL ES version " << glMajorVersion << "." << glMinorVersion << " doesn't meet the minimum required API version " << requiredMajorVersion << "." << requiredMinorVersion << " for OpenXR." << std::endl;
    }

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(GLDebugCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_FALSE);
    glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, nullptr, GL_TRUE);

#if XR_TUTORIAL_ENABLE_MULTIVIEW
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    const char* foundExtension = strstr((const char*)extensions, "GL_OVR_multiview");
    if (foundExtension == nullptr) {
        std::cerr << "ERROR: OPENGL ES: Unable to find GL_OVR_multiview extension." << std::endl;
        DEBUG_BREAK;
    }
#endif
}

OpenGLESRenderer::~OpenGLESRenderer() {
    ksGpuWindow_Destroy(&window);
}
// XR_DOCS_TAG_END_OpenGLESRenderer

void *OpenGLESRenderer::CreateDesktopSwapchain(const SwapchainCreateInfo &swapchainCI) { return nullptr; }
void OpenGLESRenderer::DestroyDesktopSwapchain(void *&swapchain) {}
void *OpenGLESRenderer::GetDesktopSwapchainImage(void *swapchain, uint32_t index) { return nullptr; }
void OpenGLESRenderer::AcquireDesktopSwapchanImage(void *swapchain, uint32_t &index) {}
void OpenGLESRenderer::PresentDesktopSwapchainImage(void *swapchain, uint32_t index) {}

size_t OpenGLESRenderer::AlignSizeForUniformBuffer(size_t size) {
    static size_t align = 0;
    if (align == 0) {
        glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, (GLint*)&align);
    }
    return Align<size_t>(size, align);
}

// XR_DOCS_TAG_BEGIN_OpenGLESRenderer_GetGraphicsBinding
void *OpenGLESRenderer::GetGraphicsBinding() {
    graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    graphicsBinding.display = window.display;
    graphicsBinding.config = window.context.config;
    graphicsBinding.context = window.context.context;
    return &graphicsBinding;
}
// XR_DOCS_TAG_END_OpenGLESRenderer_GetGraphicsBinding

// XR_DOCS_TAG_BEGIN_OpenGLESRenderer_AllocateSwapchainImageData
XrSwapchainImageBaseHeader *OpenGLESRenderer::AllocateSwapchainImageData(XrSwapchain swapchain, SwapchainType type, uint32_t count) {
    swapchainImagesMap[swapchain].first = type;
    swapchainImagesMap[swapchain].second.resize(count, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
    return reinterpret_cast<XrSwapchainImageBaseHeader *>(swapchainImagesMap[swapchain].second.data());
}
// XR_DOCS_TAG_END_OpenGLESRenderer_AllocateSwapchainImageData

void *OpenGLESRenderer::CreateImageView(const ImageViewCreateInfo &imageViewCI) {
    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);

    GLenum attachment = imageViewCI.aspect == ImageViewCreateInfo::Aspect::COLOR_BIT ? GL_COLOR_ATTACHMENT0 : GL_DEPTH_ATTACHMENT;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D_ARRAY) {
        glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, attachment, (GLuint)(uint64_t)imageViewCI.image, imageViewCI.baseMipLevel, imageViewCI.baseArrayLayer, imageViewCI.layerCount);
    } else if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D) {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attachment, GL_TEXTURE_2D, (GLuint)(uint64_t)imageViewCI.image, imageViewCI.baseMipLevel);
    } else {
        DEBUG_BREAK;
        std::cout << "ERROR: OPENGL: Unknown ImageView View type." << std::endl;
    }

    GLenum result = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (result != GL_FRAMEBUFFER_COMPLETE) {
        DEBUG_BREAK;
        std::cout << "ERROR: OPENGL: Framebuffer is not complete" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    imageViews[framebuffer] = imageViewCI;
    return (void *)(uint64_t)framebuffer;
}

void OpenGLESRenderer::DestroyImageView(void *&imageView) {
    GLuint framebuffer = (GLuint)(uint64_t)imageView;
    imageViews.erase(framebuffer);
    glDeleteFramebuffers(1, &framebuffer);
    imageView = nullptr;
}

void OpenGLESRenderer::CreatePipeline(const PipelineCreateInfo &pipelineCI) {
    // InputAssemblyState
    const InputAssemblyState &IAS = pipelineCI.inputAssemblyState;
    if (IAS.primitiveRestartEnable) {
        glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    } else {
        glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    }

    // RasterisationState
    const RasterisationState &RS = pipelineCI.rasterisationState;

    if (RS.rasteriserDiscardEnable) {
        glEnable(GL_RASTERIZER_DISCARD);
    } else {
        glDisable(GL_RASTERIZER_DISCARD);
    }

    if (RS.cullMode > CullMode::NONE) {
        glEnable(GL_CULL_FACE);
        glCullFace(ToGLCullMode(RS.cullMode));
    } else {
        glDisable(GL_CULL_FACE);
    }

    glFrontFace(RS.frontFace == FrontFace::COUNTER_CLOCKWISE ? GL_CCW : GL_CW);

    GLenum polygonOffsetMode = 0;
    switch (RS.polygonMode) {
    default:
    case PolygonMode::FILL: {
        polygonOffsetMode = GL_POLYGON_OFFSET_FILL;
        break;
    }
    case PolygonMode::LINE: {
        polygonOffsetMode = 0; //GL_POLYGON_OFFSET_LINE; //None for ES
        break;
    }
    case PolygonMode::POINT: {
        polygonOffsetMode = 0; //GL_POLYGON_OFFSET_POINT; //None for ES
        break;
    }
    }
    if (RS.depthBiasEnable) {
        glEnable(polygonOffsetMode);
        // glPolygonOffsetClamp
        glPolygonOffset(RS.depthBiasSlopeFactor, RS.depthBiasConstantFactor);
    } else {
        glDisable(polygonOffsetMode);
    }

    glLineWidth(RS.lineWidth);

    // MultisampleState
    const MultisampleState &MS = pipelineCI.multisampleState;

    if (MS.sampleShadingEnable) {
        glEnable(GL_SAMPLE_SHADING);
        glMinSampleShading(MS.minSampleShading);
    } else {
        glDisable(GL_SAMPLE_SHADING);
    }

    if (MS.sampleMask > 0) {
        glEnable(GL_SAMPLE_MASK);
        glSampleMaski(0, MS.sampleMask);
    } else {
        glDisable(GL_SAMPLE_MASK);
    }

    if (MS.alphaToCoverageEnable) {
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    } else {
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }

    // DepthStencilState
    const DepthStencilState &DSS = pipelineCI.depthStencilState;

    if (DSS.depthTestEnable) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    glDepthMask(DSS.depthWriteEnable ? GL_TRUE : GL_FALSE);

    glDepthFunc(ToGLCompareOp(DSS.depthCompareOp));

    if (DSS.stencilTestEnable) {
        glEnable(GL_STENCIL_TEST);
    } else {
        glDisable(GL_STENCIL_TEST);
    }

    glStencilOpSeparate(GL_FRONT,
                        ToGLStencilCompareOp(DSS.front.failOp),
                        ToGLStencilCompareOp(DSS.front.depthFailOp),
                        ToGLStencilCompareOp(DSS.front.passOp));
    glStencilFuncSeparate(GL_FRONT,
                          ToGLCompareOp(DSS.front.compareOp),
                          DSS.front.reference,
                          DSS.front.compareMask);
    glStencilMaskSeparate(GL_FRONT, DSS.front.writeMask);

    glStencilOpSeparate(GL_BACK,
                        ToGLStencilCompareOp(DSS.back.failOp),
                        ToGLStencilCompareOp(DSS.back.depthFailOp),
                        ToGLStencilCompareOp(DSS.back.passOp));
    glStencilFuncSeparate(GL_BACK,
                          ToGLCompareOp(DSS.back.compareOp),
                          DSS.back.reference,
                          DSS.back.compareMask);
    glStencilMaskSeparate(GL_BACK, DSS.back.writeMask);

    // ColorBlendState
    const ColorBlendState &CBS = pipelineCI.colorBlendState;

    for (int i = 0; i < (int)CBS.attachments.size(); i++) {
        const ColorBlendAttachmentState &CBA = CBS.attachments[i];

        if (CBA.blendEnable) {
            glEnablei(GL_BLEND, i);
        } else {
            glDisablei(GL_BLEND, i);
        }

        glBlendEquationSeparatei(i, ToGLBlendOp(CBA.colorBlendOp), ToGLBlendOp(CBA.alphaBlendOp));

        glBlendFuncSeparatei(i,
                             ToGLBlendFactor(CBA.srcColorBlendFactor),
                             ToGLBlendFactor(CBA.dstColorBlendFactor),
                             ToGLBlendFactor(CBA.srcAlphaBlendFactor),
                             ToGLBlendFactor(CBA.dstAlphaBlendFactor));

        glColorMaski(i,
                     (((uint32_t)CBA.colorWriteMask & (uint32_t)ColorComponentBit::R_BIT) == (uint32_t)ColorComponentBit::R_BIT),
                     (((uint32_t)CBA.colorWriteMask & (uint32_t)ColorComponentBit::G_BIT) == (uint32_t)ColorComponentBit::G_BIT),
                     (((uint32_t)CBA.colorWriteMask & (uint32_t)ColorComponentBit::B_BIT) == (uint32_t)ColorComponentBit::B_BIT),
                     (((uint32_t)CBA.colorWriteMask & (uint32_t)ColorComponentBit::A_BIT) == (uint32_t)ColorComponentBit::A_BIT));
    }
    glBlendColor(CBS.blendConstants[0], CBS.blendConstants[1], CBS.blendConstants[2], CBS.blendConstants[3]);
}

void OpenGLESRenderer::BeginRendering() {
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
}

void OpenGLESRenderer::EndRendering() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    framebuffer = 0;
}

void OpenGLESRenderer::ClearColor(void *imageView, float r, float g, float b, float a) {
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)(uint64_t)imageView);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLESRenderer::ClearDepth(void *imageView, float d) {
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)(uint64_t)imageView);
    glClearDepthf(d);
    glClear(GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLESRenderer::SetRenderAttachments(void **colorViews, size_t colorViewCount, void *depthStencilView, uint32_t width, uint32_t height) {
    // Reset Framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    framebuffer = 0;

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // Color
    for (size_t i = 0; i < colorViewCount; i++) {
        GLuint glColorView = (GLuint)(uint64_t)colorViews[i];
        const ImageViewCreateInfo &imageViewCI = imageViews[glColorView];

        if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D_ARRAY) {
            glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, (GLuint)(uint64_t)imageViewCI.image, imageViewCI.baseMipLevel, imageViewCI.baseArrayLayer, imageViewCI.layerCount);
        } else if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D) {
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, (GLuint)(uint64_t)imageViewCI.image, imageViewCI.baseMipLevel);
        } else {
            DEBUG_BREAK;
            std::cout << "ERROR: OPENGL: Unknown ImageView View type." << std::endl;
        }
    }
    // DepthStencil
    if (depthStencilView) {
        GLuint glDepthView = (GLuint)(uint64_t)depthStencilView;
        const ImageViewCreateInfo &imageViewCI = imageViews[glDepthView];

        if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D_ARRAY) {
            glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, (GLuint)(uint64_t)imageViewCI.image, imageViewCI.baseMipLevel, imageViewCI.baseArrayLayer, imageViewCI.layerCount);
        } else if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D) {
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, (GLuint)(uint64_t)imageViewCI.image, imageViewCI.baseMipLevel);
        } else {
            DEBUG_BREAK;
            std::cout << "ERROR: OPENGL: Unknown ImageView View type." << std::endl;
        }
    }

    GLenum result = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (result != GL_FRAMEBUFFER_COMPLETE) {
        DEBUG_BREAK;
        std::cout << "ERROR: OPENGL: Framebuffer is not complete." << std::endl;
    }
}

void OpenGLESRenderer::SetViewports(Viewport *viewports, size_t count) {
    Viewport viewport = viewports[0];
    glViewport((GLint)viewport.x, (GLint)viewport.y, (GLsizei)viewport.width, (GLsizei)viewport.height);
    glDepthRangef(viewport.minDepth, viewport.maxDepth);
}

void OpenGLESRenderer::SetScissors(Rect2D *scissors, size_t count) {
    Rect2D scissor = scissors[0];
    glScissor((GLint)scissor.offset.x, (GLint)scissor.offset.y, (GLsizei)scissor.extent.width, (GLsizei)scissor.extent.height);
}

// XR_DOCS_TAG_BEGIN_OpenGLESRenderer_GetSupportedSwapchainFormats
const std::vector<int64_t> OpenGLESRenderer::GetSupportedColorSwapchainFormats() {
    // https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/f122f9f1fc729e2dc82e12c3ce73efa875182854/src/tests/hello_xr/graphicsplugin_opengles.cpp#L208-L216
    GLint glMajorVersion = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &glMajorVersion);
    if (glMajorVersion >= 3) {
        return {GL_RGBA8, GL_RGBA8_SNORM, GL_SRGB8_ALPHA8};
    } else {
        return {GL_RGBA8, GL_RGBA8_SNORM};
    }
}
const std::vector<int64_t> OpenGLESRenderer::GetSupportedDepthSwapchainFormats() {
    return {
        GL_DEPTH_COMPONENT32F,
        GL_DEPTH_COMPONENT24,
        GL_DEPTH_COMPONENT16};
}
// XR_DOCS_TAG_END_OpenGLESRenderer_GetSupportedSwapchainFormats

RenderStats OpenGLESRenderer::DrawNode(Scene &scene, Camera cameras[], Node* node, const glm::mat4 &parentTransform,
                                       bool frustumCull, const Material* overrideMaterial) {
    const glm::mat4 &model = parentTransform * node->getTransformParentFromLocal();

    RenderStats stats;
    if (node->entity != nullptr) {
        if (node->visible) {
            // HACK: have to do manually bind cameras for now
            auto mesh = static_cast<Mesh*>(node->entity);
            mesh->material->bind();
            for (uint32_t i = 0; i < 2; i++) {
                mesh->material->shader->setMat4("view["+std::to_string(i)+"]", cameras[i].getViewMatrix());
                mesh->material->shader->setMat4("projection["+std::to_string(i)+"]", cameras[i].getProjectionMatrix());
            }
            mesh->material->unbind();

            node->entity->bindMaterial(scene, cameras[0], model, overrideMaterial);
            bool doFrustumCull = frustumCull && node->frustumCulled;
            stats += node->entity->draw(scene, cameras[0], model, doFrustumCull, overrideMaterial);
        }
    }

    for (auto& child : node->children) {
        stats += DrawNode(scene, cameras, child, model, overrideMaterial);
    }

    return stats;
}

#endif
