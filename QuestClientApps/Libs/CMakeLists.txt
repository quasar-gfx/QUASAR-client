cmake_minimum_required(VERSION 3.22)
set(TARGET LibApps)
project(${TARGET})

# additional Directories for find_package() to search within.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include("${CMAKE_SOURCE_DIR}/cmake/graphics_api_select.cmake")

set(LIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(OGL_REMOTE_RENDERING_DIR ${LIBS_DIR}/opengl-remote-rendering)
set(OGL_REMOTE_RENDERING_OUTPUT_DIR ${CMAKE_BINARY_DIR}/Libs/opengl-remote-rendering)

set(RENDERER_DIR ${OGL_REMOTE_RENDERING_DIR}/renderer)
set(RENDERER_SHADERS_DIR ${RENDERER_DIR}/shaders)
set(EXTERNAL_DIR ${OGL_REMOTE_RENDERING_DIR}/third_party)
set(APP_COMMON_DIR ${OGL_REMOTE_RENDERING_DIR}/apps/Common)
set(APP_COMMON_SHADERS_DIR ${APP_COMMON_DIR}/shaders)

set(FFMPEG_LIBS LibFFmpeg)

# set required libs
set(RENDERER_LIBS )

# add third party libraries

# spdlog
add_subdirectory(${EXTERNAL_DIR}/spdlog)
set(RENDERER_LIBS ${RENDERER_LIBS} spdlog)

# assimp
add_subdirectory(${EXTERNAL_DIR}/libassimp/tnt)
set(RENDERER_LIBS ${RENDERER_LIBS} assimp)

# stb
add_subdirectory(${EXTERNAL_DIR}/stb/tnt)
set(RENDERER_LIBS ${RENDERER_LIBS} stb)

# jsmn
add_subdirectory(${EXTERNAL_DIR}/jsmn/tnt)
set(RENDERER_LIBS ${RENDERER_LIBS} jsmn)

# lz4
add_subdirectory(${EXTERNAL_DIR}/lz4_stream/tnt)
set(RENDERER_LIBS ${RENDERER_LIBS} lz4_stream)

# zstd
add_subdirectory(${EXTERNAL_DIR}/zstd/build/cmake)
set(RENDERER_LIBS ${RENDERER_LIBS} libzstd_static)

# for FetchContent_Declare() and FetchContent_MakeAvailable()
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

# FFmpeg
add_subdirectory(ffmpeg-libs)

# renderer shaders
add_subdirectory(${RENDERER_SHADERS_DIR})

# common shaders
add_subdirectory(${APP_COMMON_SHADERS_DIR})

# files
set(RENDERER_SRCS
    ${RENDERER_DIR}/src/Animation.cpp
    ${RENDERER_DIR}/src/Cameras/VRCamera.cpp
    ${RENDERER_DIR}/src/Cameras/PerspectiveCamera.cpp
    ${RENDERER_DIR}/src/CubeMap.cpp
    ${RENDERER_DIR}/src/Materials/PBRMaterial.cpp
    ${RENDERER_DIR}/src/Materials/UnlitMaterial.cpp
    ${RENDERER_DIR}/src/Materials/DirShadowMapMaterial.cpp
    ${RENDERER_DIR}/src/Materials/PointShadowMapMaterial.cpp
    ${RENDERER_DIR}/src/Renderers/OpenGLRenderer.cpp
    ${RENDERER_DIR}/src/Primitives/Entity.cpp
    ${RENDERER_DIR}/src/Primitives/Mesh.cpp
    ${RENDERER_DIR}/src/Primitives/Model.cpp
    ${RENDERER_DIR}/src/Primitives/Node.cpp
    ${RENDERER_DIR}/src/Scene.cpp
    ${RENDERER_DIR}/src/SceneLoader.cpp
    ${RENDERER_DIR}/src/Shaders/ComputeShader.cpp
    ${RENDERER_DIR}/src/Shaders/ShaderBase.cpp
    ${RENDERER_DIR}/src/Shaders/Shader.cpp
    ${RENDERER_DIR}/src/Texture.cpp
    ${RENDERER_DIR}/src/Utils/FileIO.cpp
    ${RENDERER_DIR}/src/Vertex.cpp
)

set(COMMON_SRCS
    ${APP_COMMON_DIR}/src/BC4DepthVideoTexture.cpp
    ${APP_COMMON_DIR}/src/DataReceiverTCP.cpp
    ${APP_COMMON_DIR}/src/DataReceiverUDP.cpp
    ${APP_COMMON_DIR}/src/DataStreamerTCP.cpp
    ${APP_COMMON_DIR}/src/DataStreamerUDP.cpp
    ${APP_COMMON_DIR}/src/Quads/DepthOffsets.cpp
    ${APP_COMMON_DIR}/src/Quads/QuadsBuffers.cpp
    ${APP_COMMON_DIR}/src/Quads/MeshFromQuads.cpp
    ${APP_COMMON_DIR}/src/VideoTexture.cpp
    ${APP_COMMON_DIR}/src/Quads/QuadMaterial.cpp
)

# app library code
file(GLOB_RECURSE SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

add_library(${TARGET}
    STATIC
    ${SRCS}
    ${RENDERER_SRCS}
    ${COMMON_SRCS}
)

target_compile_options(${TARGET} PRIVATE -Wno-cast-calling-convention)
AddGraphicsAPIDefine(${TARGET})
target_compile_definitions(${TARGET} PRIVATE XR_TUTORIAL_ENABLE_MULTIVIEW)
# target_compile_definitions(${TARGET} PRIVATE VR)

include(${CMAKE_SOURCE_DIR}/cmake/gfxwrapper.cmake)
if(TARGET openxr-gfxwrapper)
    target_include_directories(${TARGET} PRIVATE ${openxr_SOURCE_DIR}/src/common)
    target_link_libraries(${TARGET} PRIVATE openxr-gfxwrapper)
    target_compile_definitions(${TARGET} PRIVATE XR_TUTORIAL_USE_OPENGL_ES)
endif()

target_link_libraries(${TARGET}
    PUBLIC
    android
    app_glue
    openxr_loader
    ${RENDERER_LIBS}
    ${FFMPEG_LIBS}
)

target_include_directories(${TARGET}
    PUBLIC
    # Android NDK includes
    ${ANDROID_NDK}/sources/android/native_app_glue

    # Renderer related includes
    ${RENDERER_DIR}/include
    ${OGL_REMOTE_RENDERING_DIR}/apps/Common/include
    ${EXTERNAL_DIR}
    ${EXTERNAL_DIR}/spdlog/include
    ${OGL_REMOTE_RENDERING_OUTPUT_DIR}/renderer/shaders
    ${OGL_REMOTE_RENDERING_OUTPUT_DIR}/apps/Common/shaders

    # From OpenXR repo
    ${openxr_SOURCE_DIR}/src/common
    ${openxr_SOURCE_DIR}/external/include

    # From this project
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
