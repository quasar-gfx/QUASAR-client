cmake_minimum_required(VERSION 3.22)
project(cpp)

set(OGL_REMOTE_RENDERING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/opengl-remote-rendering)
set(OGL_REMOTE_RENDERING_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/libs/opengl-remote-rendering)

set(RENDERER_DIR ${OGL_REMOTE_RENDERING_DIR}/renderer)
set(RENDERER_SHADERS_DIR ${RENDERER_DIR}/shaders)
set(EXTERNAL_DIR ${OGL_REMOTE_RENDERING_DIR}/third_party)
set(APP_COMMON_DIR ${OGL_REMOTE_RENDERING_DIR}/apps/Common)
set(APP_COMMON_SHADERS_DIR ${APP_COMMON_DIR}/shaders)

# additional Directories for find_package() to search within.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include("${CMAKE_SOURCE_DIR}/cmake/graphics_api_select.cmake")

# set required libs
set(RENDERER_LIBS )

# add third party libraries
add_subdirectory(${EXTERNAL_DIR}/libassimp/tnt)
set(RENDERER_LIBS ${RENDERER_LIBS} assimp)

add_subdirectory(${EXTERNAL_DIR}/stb/tnt)
set(RENDERER_LIBS ${RENDERER_LIBS} stb)

add_subdirectory(${EXTERNAL_DIR}/jsmn/tnt)
set(RENDERER_LIBS ${RENDERER_LIBS} jsmn)

# For FetchContent_Declare() and FetchContent_MakeAvailable()
include(FetchContent)

# openxr_loader - From github.com/KhronosGroup
set(BUILD_TESTS
    OFF
    CACHE INTERNAL "Build tests"
)
set(BUILD_API_LAYERS
    ON
    CACHE INTERNAL "Use OpenXR layers"
)
FetchContent_Declare(
    OpenXR
    URL_HASH MD5=924a94a2da0b5ef8e82154c623d88644
    URL https://github.com/KhronosGroup/OpenXR-SDK-Source/archive/refs/tags/release-1.0.34.zip
        SOURCE_DIR
        openxr
)
FetchContent_MakeAvailable(OpenXR)
set(RENDERER_LIBS ${RENDERER_LIBS} openxr_loader)

# FFMPEG-android-maker
set(FFMPEG_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/ffmpeg")
set(FFMPEG_LIBS_DIR ${FFMPEG_ROOT_DIR}/lib/${ANDROID_ABI})
set(FFMPEG_LIBS
    # ffmpeg lib names
    avutil avformat avcodec swscale swresample
)

foreach(LIB_NAME ${FFMPEG_LIBS})
    add_library(${LIB_NAME} SHARED IMPORTED)
    set_target_properties(${LIB_NAME} PROPERTIES IMPORTED_LOCATION ${FFMPEG_LIBS_DIR}/lib${LIB_NAME}.so)
endforeach()

# renderer shaders
add_subdirectory(${RENDERER_SHADERS_DIR})

# common shaders
add_subdirectory(${APP_COMMON_SHADERS_DIR})

# our shaders
add_subdirectory(shaders)

# files
set(RENDERER_HDRS
    ${RENDERER_DIR}/include/OpenGLAppConfig.h
    ${RENDERER_DIR}/include/Culling/AABB.h
    ${RENDERER_DIR}/include/Culling/BoundingSphere.h
    ${RENDERER_DIR}/include/Culling/Frustum.h
    ${RENDERER_DIR}/include/Cameras/Camera.h
    ${RENDERER_DIR}/include/Cameras/PerspectiveCamera.h
    ${RENDERER_DIR}/include/Cameras/VRCamera.h
    ${RENDERER_DIR}/include/CubeMap.h
    ${RENDERER_DIR}/include/Framebuffer.h
    ${RENDERER_DIR}/include/Materials/Material.h
    ${RENDERER_DIR}/include/Materials/PBRMaterial.h
    ${RENDERER_DIR}/include/Materials/UnlitMaterial.h
    ${RENDERER_DIR}/include/Materials/DirShadowMapMaterial.h
    ${RENDERER_DIR}/include/Materials/PointShadowMapMaterial.h
    ${RENDERER_DIR}/include/Lights/AmbientLight.h
    ${RENDERER_DIR}/include/Lights/Light.h
    ${RENDERER_DIR}/include/Lights/DirectionalLight.h
    ${RENDERER_DIR}/include/Lights/PointLight.h
    ${RENDERER_DIR}/include/OpenGLObject.h
    ${RENDERER_DIR}/include/Renderers/OpenGLRenderer.h
    ${RENDERER_DIR}/include/Primatives/Cube.h
    ${RENDERER_DIR}/include/Primatives/Entity.h
    ${RENDERER_DIR}/include/Primatives/FullScreenQuad.h
    ${RENDERER_DIR}/include/Primatives/Mesh.h
    ${RENDERER_DIR}/include/Primatives/Model.h
    ${RENDERER_DIR}/include/Primatives/Node.h
    ${RENDERER_DIR}/include/Primatives/Plane.h
    ${RENDERER_DIR}/include/Primatives/Sphere.h
    ${RENDERER_DIR}/include/RenderTargets/RenderTarget.h
    ${RENDERER_DIR}/include/RenderTargets/RenderTargetBase.h
    ${RENDERER_DIR}/include/Renderbuffer.h
    ${RENDERER_DIR}/include/Scene.h
    ${RENDERER_DIR}/include/SceneLoader.h
    ${RENDERER_DIR}/include/Shaders/ComputeShader.h
    ${RENDERER_DIR}/include/Shaders/Shader.h
    ${RENDERER_DIR}/include/Shaders/ShaderBase.h
    ${RENDERER_DIR}/include/Texture.h
    ${RENDERER_DIR}/include/Utils/FileIO.h
    ${RENDERER_DIR}/include/Utils/Platform.h
    ${RENDERER_DIR}/include/Utils/TimeUtils.h
    ${RENDERER_DIR}/include/Vertex.h
)
set(RENDERER_SRCS
    ${RENDERER_DIR}/src/Cameras/VRCamera.cpp
    ${RENDERER_DIR}/src/Cameras/PerspectiveCamera.cpp
    ${RENDERER_DIR}/src/CubeMap.cpp
    ${RENDERER_DIR}/src/Materials/PBRMaterial.cpp
    ${RENDERER_DIR}/src/Materials/UnlitMaterial.cpp
    ${RENDERER_DIR}/src/Materials/DirShadowMapMaterial.cpp
    ${RENDERER_DIR}/src/Materials/PointShadowMapMaterial.cpp
    ${RENDERER_DIR}/src/Renderers/OpenGLRenderer.cpp
    ${RENDERER_DIR}/src/Primatives/Entity.cpp
    ${RENDERER_DIR}/src/Primatives/Mesh.cpp
    ${RENDERER_DIR}/src/Primatives/Model.cpp
    ${RENDERER_DIR}/src/Primatives/Node.cpp
    ${RENDERER_DIR}/src/Scene.cpp
    ${RENDERER_DIR}/src/SceneLoader.cpp
    ${RENDERER_DIR}/src/Shaders/ComputeShader.cpp
    ${RENDERER_DIR}/src/Shaders/Shader.cpp
    ${RENDERER_DIR}/src/Texture.cpp
    ${RENDERER_DIR}/src/Utils/FileIO.cpp
    ${RENDERER_DIR}/src/Vertex.cpp
)

set(COMMON_HDRS
    ${APP_COMMON_DIR}/include/Networking/DataPacketUDP.h
    ${APP_COMMON_DIR}/include/Networking/DataReceiverTCP.h
    ${APP_COMMON_DIR}/include/Networking/DataReceiverUDP.h
    ${APP_COMMON_DIR}/include/Networking/DataStreamerTCP.h
    ${APP_COMMON_DIR}/include/Networking/DataStreamerUDP.h
    ${APP_COMMON_DIR}/include/Networking/Socket.h
    ${APP_COMMON_DIR}/include/Utils/Utils.h
    ${APP_COMMON_DIR}/include/CameraPose.h
    ${APP_COMMON_DIR}/include/PoseReceiver.h
    ${APP_COMMON_DIR}/include/PoseStreamer.h
    ${APP_COMMON_DIR}/include/VideoTexture.h
    ${APP_COMMON_DIR}/include/QuadMaterial.h
    ${APP_COMMON_DIR}/include/BC4DepthVideoTexture.h
)
set(COMMON_SRCS
    ${APP_COMMON_DIR}/src/DataReceiverTCP.cpp
    ${APP_COMMON_DIR}/src/DataReceiverUDP.cpp
    ${APP_COMMON_DIR}/src/DataStreamerTCP.cpp
    ${APP_COMMON_DIR}/src/DataStreamerUDP.cpp
    ${APP_COMMON_DIR}/src/VideoTexture.cpp
    ${APP_COMMON_DIR}/src/QuadMaterial.cpp
    ${APP_COMMON_DIR}/src/BC4DepthVideoTexture.cpp
)

# our code
file(GLOB_RECURSE PRIVATE_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
file(GLOB_RECURSE SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(${TARGET} SHARED ${SRCS} ${PRIVATE_HDRS} ${RENDERER_SRCS} ${RENDERER_HDRS} ${COMMON_SRCS} ${COMMON_HDRS})
target_include_directories(${TARGET} PRIVATE
    # In this project
    ${CMAKE_CURRENT_SOURCE_DIR}/include

    # Renderer related includes
    ${RENDERER_DIR}/include
    ${OGL_REMOTE_RENDERING_DIR}/apps/Common/include
    ${EXTERNAL_DIR}
    ${OGL_REMOTE_RENDERING_OUTPUT_DIR}/renderer/shaders
    ${OGL_REMOTE_RENDERING_OUTPUT_DIR}/apps/Common/shaders

    # For ffmpeg-android
    libs/ffmpeg/include/${ANDROID_ABI}

    # From OpenXR repo
    ${openxr_SOURCE_DIR}/src/common
    ${openxr_SOURCE_DIR}/external/include
)

add_dependencies(${TARGET} shaders_builtin shaders_common)

add_definitions(-DVR)

# export ANativeActivity_onCreate for java to call.
set_property(
    TARGET ${TARGET}
    APPEND_STRING
    PROPERTY LINK_FLAGS " -u ANativeActivity_onCreate"
)

# native_app_glue
include(AndroidNdkModules)
android_ndk_import_module_native_app_glue()
set(ANDROID_LIBS android native_app_glue)

target_link_libraries(${TARGET} ${ANDROID_LIBS} ${RENDERER_LIBS} ${FFMPEG_LIBS})
target_compile_options(${TARGET} PRIVATE -Wno-cast-calling-convention)
AddGraphicsAPIDefine(${TARGET})
target_compile_definitions(${TARGET} PUBLIC XR_TUTORIAL_ENABLE_MULTIVIEW)

include(${CMAKE_SOURCE_DIR}/cmake/gfxwrapper.cmake)
if(TARGET openxr-gfxwrapper)
    target_include_directories(${TARGET} PUBLIC ${openxr_SOURCE_DIR}/src/common)
    target_link_libraries(${TARGET} openxr-gfxwrapper)
    target_compile_definitions(${TARGET} PUBLIC XR_TUTORIAL_USE_OPENGL_ES)
endif()
