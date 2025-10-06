#include <OpenGLESRenderer.h>

#include <Utils/DebugOutput.h>
#include <Utils/OpenXRDebugUtils.h>

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

using namespace quasar;

int64_t OpenGLESRenderer::SelectColorSwapchainFormat(const std::vector<int64_t>& formats) {
    const std::vector<int64_t>& supportSwapchainFormats = GetSupportedColorSwapchainFormats();

    const std::vector<int64_t>::const_iterator& swapchainFormatIt = std::find_first_of(formats.begin(), formats.end(),
                                                                                       std::begin(supportSwapchainFormats), std::end(supportSwapchainFormats));
    if (swapchainFormatIt == formats.end()) {
        spdlog::error("ERROR: Unable to find supported Color Swapchain Format");
        DEBUG_BREAK;
        return 0;
    }

    return *swapchainFormatIt;
}

int64_t OpenGLESRenderer::SelectDepthSwapchainFormat(const std::vector<int64_t>& formats) {
    const std::vector<int64_t>& supportSwapchainFormats = GetSupportedDepthSwapchainFormats();

    const std::vector<int64_t>::const_iterator &swapchainFormatIt = std::find_first_of(formats.begin(), formats.end(),
                                                                                       std::begin(supportSwapchainFormats), std::end(supportSwapchainFormats));
    if (swapchainFormatIt == formats.end()) {
        spdlog::error("ERROR: Unable to find supported Depth Swapchain Format");
        DEBUG_BREAK;
        return 0;
    }

    return *swapchainFormatIt;
}

OpenGLESRenderer::OpenGLESRenderer(const Config& config, XrInstance xrInstance, XrSystemId systemId)
    : OpenGLRenderer(config)
{
    // https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/f122f9f1fc729e2dc82e12c3ce73efa875182854/src/tests/hello_xr/graphicsplugin_openglescpp#L101-L119
    // Initialize the gl extensions. Note we have to open a window
    ksDriverInstance driverInstance{};
    ksGpuQueueInfo queueInfo{};
    ksGpuSurfaceColorFormat colorFormat{KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8};
    ksGpuSurfaceDepthFormat depthFormat{KS_GPU_SURFACE_DEPTH_FORMAT_D24};
    ksGpuSampleCount sampleCount{KS_GPU_SAMPLE_COUNT_1};
    if (!ksGpuWindow_Create(&window, &driverInstance, &queueInfo, 0, colorFormat, depthFormat, sampleCount, 640, 480, false)) {
        spdlog::error("ERROR: OPENGL ES: Failed to create Context.");
    }

    OPENXR_CHECK(xrGetInstanceProcAddr(xrInstance, "xrGetOpenGLESGraphicsRequirementsKHR",
                    (PFN_xrVoidFunction*)&xrGetOpenGLESGraphicsRequirementsKHR), "Failed to get InstanceProcAddr for xrGetOpenGLESGraphicsRequirementsKHR.");
    XrGraphicsRequirementsOpenGLESKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
    OPENXR_CHECK(xrGetOpenGLESGraphicsRequirementsKHR(xrInstance, systemId, &graphicsRequirements), "Failed to get Graphics Requirements for OpenGLES.");

    GLint glMajorVersion = 0;
    GLint glMinorVersion = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &glMajorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &glMinorVersion);

    const XrVersion glApiVersion = XR_MAKE_VERSION(glMajorVersion, glMinorVersion, 0);
    if (graphicsRequirements.minApiVersionSupported > glApiVersion) {
        int requiredMajorVersion = XR_VERSION_MAJOR(graphicsRequirements.minApiVersionSupported);
        int requiredMinorVersion = XR_VERSION_MINOR(graphicsRequirements.minApiVersionSupported);
        spdlog::error("ERROR: OPENGL ES: The created OpenGL ES version {}.{} doesn't meet the minimum required API version {}.{} for OpenXR.", glMajorVersion, glMinorVersion, requiredMajorVersion, requiredMinorVersion);
    }

    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    const char* foundExtension = strstr((const char*)extensions, "GL_OVR_multiview");
    if (foundExtension == nullptr) {
        spdlog::error("ERROR: OPENGL ES: Unable to find GL_OVR_multiview extension.");
        DEBUG_BREAK;
    }

    // Have to recreate resources here since some resources are created in the parent constructor and
    // we need to wait until after we have a valid GL context
    skyboxShader = Shader({
        .vertexCodeData = SHADER_BUILTIN_SKYBOX_VERT,
        .vertexCodeSize = SHADER_BUILTIN_SKYBOX_VERT_len,
        .fragmentCodeData = SHADER_BUILTIN_SKYBOX_FRAG,
        .fragmentCodeSize = SHADER_BUILTIN_SKYBOX_FRAG_len,
    });
    pointLightsUBO = Buffer({
        .target = GL_UNIFORM_BUFFER,
        .dataSize = sizeof(Scene::GPUPointLightBlock),
        .numElems = 1,
        .usage = GL_DYNAMIC_DRAW,
    });

    outputFsQuad = std::make_unique<FullScreenQuad>();
}

OpenGLESRenderer::~OpenGLESRenderer() {
    ksGpuWindow_Destroy(&window);
}

void* OpenGLESRenderer::GetGraphicsBinding() {
    graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    graphicsBinding.display = window.display;
    graphicsBinding.config = window.context.config;
    graphicsBinding.context = window.context.context;
    return &graphicsBinding;
}

XrSwapchainImageBaseHeader* OpenGLESRenderer::AllocateSwapchainImageData(XrSwapchain swapchain, SwapchainType type, uint32_t count) {
    swapchainImagesMap[swapchain].first = type;
    swapchainImagesMap[swapchain].second.resize(count, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
    return reinterpret_cast<XrSwapchainImageBaseHeader *>(swapchainImagesMap[swapchain].second.data());
}

void* OpenGLESRenderer::CreateImageView(const ImageViewCreateInfo &imageViewCI) {
    auto framebuffer = std::make_unique<MultiviewFramebuffer>();
    framebuffer->bind();

    GLenum attachment = imageViewCI.aspect == ImageViewCreateInfo::Aspect::COLOR_BIT ? GL_COLOR_ATTACHMENT0 : GL_DEPTH_ATTACHMENT;

    if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D_ARRAY) {
        framebuffer->attachTextureMultiview((GLuint)(uint64_t)imageViewCI.image, attachment, imageViewCI.baseMipLevel, imageViewCI.baseArrayLayer, imageViewCI.layerCount);
    }
    else if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D) {
        framebuffer->attachTexture((GLuint)(uint64_t)imageViewCI.image, attachment, imageViewCI.baseMipLevel);
    }
    else {
        spdlog::error("ERROR: OPENGL: Unknown ImageView View type.");
        DEBUG_BREAK;
    }

    if (!framebuffer->checkStatus("CreateImageView")) {
        DEBUG_BREAK;
    }
    framebuffer->unbind();

    GLuint framebufferID = framebuffer->ID;
    imageViews[framebufferID] = imageViewCI;
    imageViewFramebuffers[framebufferID] = std::move(framebuffer);
    return (void*)(uint64_t)framebufferID;
}

void OpenGLESRenderer::DestroyImageView(void* &imageView) {
    GLuint framebufferID = (GLuint)(uint64_t)imageView;
    imageViews.erase(framebufferID);
    imageViewFramebuffers.erase(framebufferID);
    imageView = nullptr;
}

void OpenGLESRenderer::beginRendering() {
    if (outputRT == nullptr) {
        outputRT = std::make_unique<MultiviewRenderTarget>(RenderTargetCreateParams{
            .width = width,
            .height = height,
        });
    }
    outputRT->bind();
}

void OpenGLESRenderer::endRendering() {
    outputRT->unbind();
}

void OpenGLESRenderer::SetRenderAttachments(void** colorViews, size_t colorViewCount, void* depthStencilView, uint32_t width, uint32_t height) {
    // Reset framebuffer
    if (displayFBO) {
        displayFBO.reset();
    }

    // Create new framebuffer
    displayFBO = std::make_unique<MultiviewFramebuffer>();
    displayFBO->bind();

    // Color
    for (size_t i = 0; i < colorViewCount; i++) {
        GLuint glColorView = (GLuint)(uint64_t)colorViews[i];
        const ImageViewCreateInfo& imageViewCI = imageViews[glColorView];

        if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D_ARRAY) {
            displayFBO->attachTextureMultiview((GLuint)(uint64_t)imageViewCI.image, GL_COLOR_ATTACHMENT0, imageViewCI.baseMipLevel, imageViewCI.baseArrayLayer, imageViewCI.layerCount);
        }
        else if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D) {
            displayFBO->attachTexture((GLuint)(uint64_t)imageViewCI.image, GL_COLOR_ATTACHMENT0, imageViewCI.baseMipLevel);
        }
        else {
            spdlog::error("ERROR: OPENGL: Unknown ImageView View type.");
            DEBUG_BREAK;
        }
    }
    // DepthStencil
    if (depthStencilView) {
        GLuint glDepthView = (GLuint)(uint64_t)depthStencilView;
        const ImageViewCreateInfo& imageViewCI = imageViews[glDepthView];

        if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D_ARRAY) {
            displayFBO->attachTextureMultiview((GLuint)(uint64_t)imageViewCI.image, GL_DEPTH_ATTACHMENT, imageViewCI.baseMipLevel, imageViewCI.baseArrayLayer, imageViewCI.layerCount);
        }
        else if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D) {
            displayFBO->attachTexture((GLuint)(uint64_t)imageViewCI.image, GL_DEPTH_ATTACHMENT, imageViewCI.baseMipLevel);
        }
        else {
            spdlog::error("ERROR: OPENGL: Unknown ImageView View type.");
            DEBUG_BREAK;
        }
    }

    if (!displayFBO->checkStatus("SetRenderAttachments")) {
        DEBUG_BREAK;
    }
}

void OpenGLESRenderer::setScreenShaderUniforms(const Shader& screenShader) {
    screenShader.setTexture("screenColor", outputRT->colorTexture, 0);
    screenShader.setTexture("screenDepth", outputRT->depthStencilTexture, 1);
}

RenderStats OpenGLESRenderer::drawToScreen(const Shader& screenShader, const RenderTargetBase* overrideRenderTarget) {
    pipeline.apply();

    if (overrideRenderTarget != nullptr) {
        overrideRenderTarget->bind();
    }
    else {
        displayFBO->bind();
        glViewport(0, 0, windowWidth, windowHeight);
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    screenShader.bind();
    RenderStats stats = outputFsQuad->draw();

    if (overrideRenderTarget != nullptr) {
        overrideRenderTarget->unbind();
    }

    return stats;
}

const std::vector<int64_t> OpenGLESRenderer::GetSupportedColorSwapchainFormats() {
    // https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/f122f9f1fc729e2dc82e12c3ce73efa875182854/src/tests/hello_xr/graphicsplugin_openglescpp#L208-L216
    GLint glMajorVersion = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &glMajorVersion);
    if (glMajorVersion >= 3) {
        return {GL_RGBA8, GL_RGBA8_SNORM, GL_SRGB8_ALPHA8};
    }
    else {
        return {GL_RGBA8, GL_RGBA8_SNORM};
    }
}

const std::vector<int64_t> OpenGLESRenderer::GetSupportedDepthSwapchainFormats() {
    return {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT16};
}

#endif
