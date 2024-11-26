cmake_minimum_required(VERSION 3.22)
project(QuestClientApps)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

add_compile_options(-Wunused-variable)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

# additional Directories for find_package() to search within.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include("${CMAKE_SOURCE_DIR}/cmake/graphics_api_select.cmake")

set(LIBS_DIR ${CMAKE_SOURCE_DIR}/libs)

set(OGL_REMOTE_RENDERING_DIR ${LIBS_DIR}/opengl-remote-rendering)
set(OGL_REMOTE_RENDERING_OUTPUT_DIR ${CMAKE_BINARY_DIR}/libs/opengl-remote-rendering)

set(RENDERER_DIR ${OGL_REMOTE_RENDERING_DIR}/renderer)
set(RENDERER_SHADERS_DIR ${RENDERER_DIR}/shaders)
set(EXTERNAL_DIR ${OGL_REMOTE_RENDERING_DIR}/third_party)
set(APP_COMMON_DIR ${OGL_REMOTE_RENDERING_DIR}/apps/Common)
set(APP_COMMON_SHADERS_DIR ${APP_COMMON_DIR}/shaders)

# set required libs
set(RENDERER_LIBS )

# add third party libraries
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
set(RENDERER_LIBS ${RENDERER_LIBS} openxr_loader)

# renderer shaders
add_subdirectory(${RENDERER_SHADERS_DIR})

# common shaders
add_subdirectory(${APP_COMMON_SHADERS_DIR})

# files
set(RENDERER_SRCS
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
    ${RENDERER_DIR}/src/Shaders/Shader.cpp
    ${RENDERER_DIR}/src/Texture.cpp
    ${RENDERER_DIR}/src/Utils/FileIO.cpp
    ${RENDERER_DIR}/src/Vertex.cpp
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

# FFMPEG-android-maker
set(FFMPEG_ROOT_DIR ${LIBS_DIR}/ffmpeg)
set(FFMPEG_INCLUDE_DIR ${FFMPEG_ROOT_DIR}/include)
set(FFMPEG_LIBS_DIR ${FFMPEG_ROOT_DIR}/lib/${ANDROID_ABI})
set(FFMPEG_LIBS
    # ffmpeg lib names
    avutil avformat avcodec swscale swresample
)

foreach(LIB_NAME ${FFMPEG_LIBS})
    add_library(${LIB_NAME} SHARED IMPORTED)
    set_target_properties(${LIB_NAME} PROPERTIES IMPORTED_LOCATION ${FFMPEG_LIBS_DIR}/lib${LIB_NAME}.so)
endforeach()

include_directories(
    # Renderer related includes
    ${RENDERER_DIR}/include
    ${OGL_REMOTE_RENDERING_DIR}/apps/Common/include
    ${EXTERNAL_DIR}
    ${OGL_REMOTE_RENDERING_OUTPUT_DIR}/renderer/shaders
    ${OGL_REMOTE_RENDERING_OUTPUT_DIR}/apps/Common/shaders

    # For ffmpeg-android
    ${FFMPEG_INCLUDE_DIR}/${ANDROID_ABI}

    # From OpenXR repo
    ${openxr_SOURCE_DIR}/src/common
    ${openxr_SOURCE_DIR}/external/include
)

add_definitions(-DVR)

# add our code
add_subdirectory(Apps)
