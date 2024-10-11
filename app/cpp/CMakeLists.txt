cmake_minimum_required(VERSION 3.22)
project(cpp)

set(OGL_REMOTE_RENDERING ${CMAKE_CURRENT_SOURCE_DIR}/libs/opengl-remote-rendering)

set(RENDERER ${OGL_REMOTE_RENDERING}/renderer)
set(SHADERS ${OGL_REMOTE_RENDERING}/shaders)
set(EXTERNAL ${OGL_REMOTE_RENDERING}/third_party)

# additional Directories for find_package() to search within.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include("${CMAKE_SOURCE_DIR}/cmake/graphics_api_select.cmake")

# set required libs
set(RENDERER_LIBS )

# add third party libraries
add_subdirectory(${EXTERNAL}/libassimp/tnt)
set(RENDERER_LIBS ${RENDERER_LIBS} assimp)

add_subdirectory(${EXTERNAL}/stb/tnt)
set(RENDERER_LIBS ${RENDERER_LIBS} stb)

add_subdirectory(${EXTERNAL}/jsmn/tnt)
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
add_subdirectory(${SHADERS})

# our shaders
add_subdirectory(shaders)

# files
set(RENDERER_HDRS
    ${RENDERER}/include/OpenGLAppConfig.h
    ${RENDERER}/include/Culling/AABB.h
    ${RENDERER}/include/Culling/BoundingSphere.h
    ${RENDERER}/include/Culling/Frustum.h
    ${RENDERER}/include/Cameras/Camera.h
    ${RENDERER}/include/Cameras/PerspectiveCamera.h
    ${RENDERER}/include/Cameras/VRCamera.h
    ${RENDERER}/include/CubeMap.h
    ${RENDERER}/include/Framebuffer.h
    ${RENDERER}/include/Materials/Material.h
    ${RENDERER}/include/Materials/PBRMaterial.h
    ${RENDERER}/include/Materials/UnlitMaterial.h
    ${RENDERER}/include/Materials/DirShadowMapMaterial.h
    ${RENDERER}/include/Materials/PointShadowMapMaterial.h
    ${RENDERER}/include/Lights/AmbientLight.h
    ${RENDERER}/include/Lights/Light.h
    ${RENDERER}/include/Lights/DirectionalLight.h
    ${RENDERER}/include/Lights/PointLight.h
    ${RENDERER}/include/OpenGLObject.h
    ${RENDERER}/include/Renderers/OpenGLRenderer.h
    ${RENDERER}/include/Primatives/Cube.h
    ${RENDERER}/include/Primatives/Entity.h
    ${RENDERER}/include/Primatives/FullScreenQuad.h
    ${RENDERER}/include/Primatives/Mesh.h
    ${RENDERER}/include/Primatives/Model.h
    ${RENDERER}/include/Primatives/Node.h
    ${RENDERER}/include/Primatives/Plane.h
    ${RENDERER}/include/Primatives/Sphere.h
    ${RENDERER}/include/RenderTargets/RenderTarget.h
    ${RENDERER}/include/RenderTargets/RenderTargetBase.h
    ${RENDERER}/include/Renderbuffer.h
    ${RENDERER}/include/Scene.h
    ${RENDERER}/include/SceneLoader.h
    ${RENDERER}/include/Shaders/ComputeShader.h
    ${RENDERER}/include/Shaders/Shader.h
    ${RENDERER}/include/Shaders/ShaderBase.h
    ${RENDERER}/include/Texture.h
    ${RENDERER}/include/Utils/FileIO.h
    ${RENDERER}/include/Utils/Platform.h
    ${RENDERER}/include/Utils/TimeUtils.h
    ${RENDERER}/include/Vertex.h
)
set(RENDERER_SRCS
    ${RENDERER}/src/Cameras/VRCamera.cpp
    ${RENDERER}/src/Cameras/PerspectiveCamera.cpp
    ${RENDERER}/src/CubeMap.cpp
    ${RENDERER}/src/Materials/PBRMaterial.cpp
    ${RENDERER}/src/Materials/UnlitMaterial.cpp
    ${RENDERER}/src/Materials/DirShadowMapMaterial.cpp
    ${RENDERER}/src/Materials/PointShadowMapMaterial.cpp
    ${RENDERER}/src/Renderers/OpenGLRenderer.cpp
    ${RENDERER}/src/Primatives/Entity.cpp
    ${RENDERER}/src/Primatives/Mesh.cpp
    ${RENDERER}/src/Primatives/Model.cpp
    ${RENDERER}/src/Primatives/Node.cpp
    ${RENDERER}/src/Scene.cpp
    ${RENDERER}/src/SceneLoader.cpp
    ${RENDERER}/src/Shaders/ComputeShader.cpp
    ${RENDERER}/src/Shaders/Shader.cpp
    ${RENDERER}/src/Texture.cpp
    ${RENDERER}/src/Utils/FileIO.cpp
    ${RENDERER}/src/Vertex.cpp
)

file(GLOB_RECURSE COMMON_HDRS
    "${OGL_REMOTE_RENDERING}/apps/Common/include/*.h"
)
file(GLOB_RECURSE COMMON_SRCS
    "${OGL_REMOTE_RENDERING}/apps/Common/src/*.cpp"
)

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
    ${RENDERER}/include
    ${OGL_REMOTE_RENDERING}/apps/Common/include
    ${EXTERNAL}
    ${CMAKE_BINARY_DIR}/shaders

    # For ffmpeg-android
    "libs/ffmpeg/include/${ANDROID_ABI}"

    # From OpenXR repo
    "${openxr_SOURCE_DIR}/src/common"
    "${openxr_SOURCE_DIR}/external/include"
)

add_dependencies(${TARGET} shaders)

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
