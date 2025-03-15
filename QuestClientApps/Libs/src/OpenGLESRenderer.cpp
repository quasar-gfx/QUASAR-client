#include <Shaders/Shader.h>
#include <Primitives/Mesh.h>
#include <Primitives/Model.h>

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

OpenGLESRenderer::OpenGLESRenderer(const Config &config, XrInstance m_xrInstance, XrSystemId systemId) : GraphicsAPI(config) {
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

#if QUEST_CLIENT_ENABLE_MULTIVIEW
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

void *OpenGLESRenderer::GetGraphicsBinding() {
    graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    graphicsBinding.display = window.display;
    graphicsBinding.config = window.context.config;
    graphicsBinding.context = window.context.context;
    return &graphicsBinding;
}

XrSwapchainImageBaseHeader *OpenGLESRenderer::AllocateSwapchainImageData(XrSwapchain swapchain, SwapchainType type, uint32_t count) {
    swapchainImagesMap[swapchain].first = type;
    swapchainImagesMap[swapchain].second.resize(count, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
    return reinterpret_cast<XrSwapchainImageBaseHeader *>(swapchainImagesMap[swapchain].second.data());
}

void *OpenGLESRenderer::CreateImageView(const ImageViewCreateInfo &imageViewCI) {
    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);

    GLenum attachment = imageViewCI.aspect == ImageViewCreateInfo::Aspect::COLOR_BIT ? GL_COLOR_ATTACHMENT0 : GL_DEPTH_ATTACHMENT;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D_ARRAY) {
        glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, attachment, (GLuint)(uint64_t)imageViewCI.image, imageViewCI.baseMipLevel, imageViewCI.baseArrayLayer, imageViewCI.layerCount);
    }
    else if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D) {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attachment, GL_TEXTURE_2D, (GLuint)(uint64_t)imageViewCI.image, imageViewCI.baseMipLevel);
    }
    else {
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

void OpenGLESRenderer::beginRendering() {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
}

void OpenGLESRenderer::endRendering() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLESRenderer::SetRenderAttachments(void **colorViews, size_t colorViewCount, void *depthStencilView, uint32_t width, uint32_t height) {
    // Reset Framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    framebuffer = 0;

    // Create New Framebuffer
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // Color
    for (size_t i = 0; i < colorViewCount; i++) {
        GLuint glColorView = (GLuint)(uint64_t)colorViews[i];
        const ImageViewCreateInfo &imageViewCI = imageViews[glColorView];

        if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D_ARRAY) {
            glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, (GLuint)(uint64_t)imageViewCI.image, imageViewCI.baseMipLevel, imageViewCI.baseArrayLayer, imageViewCI.layerCount);
        }
        else if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D) {
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, (GLuint)(uint64_t)imageViewCI.image, imageViewCI.baseMipLevel);
        }
        else {
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
        }
        else if (imageViewCI.view == ImageViewCreateInfo::View::TYPE_2D) {
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, (GLuint)(uint64_t)imageViewCI.image, imageViewCI.baseMipLevel);
        }
        else {
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

RenderStats OpenGLESRenderer::drawObjects(const Scene &scene, const Camera &camera, uint32_t clearMask) {
    pipeline.apply();

    RenderStats stats;

    updateDirLightShadow(scene, camera);
    // point light shadows are not implemented yet

    // draw all objects in the scene
    glViewport(0, 0, width, height); // restore viewport
    stats += GraphicsAPI::drawScene(scene, camera, clearMask);

    // draw lights for debugging
    stats += GraphicsAPI::drawLights(scene, camera);

    // draw skybox
    stats += GraphicsAPI::drawSkyBox(scene, camera);

    return stats;
}

void OpenGLESRenderer::setScreenShaderUniforms(const Shader &screenShader) {
    // screenShader.setTexture("screenColor", gBuffer.colorBuffer, 0);
    // screenShader.setTexture("screenDepth", gBuffer.depthBuffer, 1);
}

RenderStats OpenGLESRenderer::drawToScreen(const Shader &screenShader, const RenderTargetBase* overrideRenderTarget) {
    pipeline.apply();

    FullScreenQuad outputFsQuad;

    beginRendering();

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    screenShader.bind();
    RenderStats stats = outputFsQuad.draw();
    screenShader.unbind();

    endRendering();

    return stats;
}

const std::vector<int64_t> OpenGLESRenderer::GetSupportedColorSwapchainFormats() {
    // https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/f122f9f1fc729e2dc82e12c3ce73efa875182854/src/tests/hello_xr/graphicsplugin_opengles.cpp#L208-L216
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
