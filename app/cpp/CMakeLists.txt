cmake_minimum_required(VERSION 3.22)
project(cpp)

set(OGL_REMOTE_RENDERING ${CMAKE_CURRENT_SOURCE_DIR}/opengl-remote-rendering)

set(RENDERER ${OGL_REMOTE_RENDERING}/renderer)
set(SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(EXTERNAL ${OGL_REMOTE_RENDERING}/third_party)

# Additional Directories for find_package() to search within.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include("${CMAKE_SOURCE_DIR}/cmake/graphics_api_select.cmake")

message("Using graphics API: " ${XR_TUTORIAL_GRAPHICS_API})

# set required libs
set(RENDERER_LIBS )

# For FetchContent_Declare() and FetchContent_MakeAvailable()
include(FetchContent)

# add third party libraries
add_subdirectory(${EXTERNAL}/stb/tnt)
set(RENDERER_LIBS ${RENDERER_LIBS} stb)

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

add_subdirectory(${SHADERS})

# Files
set(RENDERER_HDRS
    ${RENDERER}/include/Culling/AABB.h
    ${RENDERER}/include/Culling/BoundingSphere.h
    ${RENDERER}/include/Culling/Frustum.h
    ${RENDERER}/include/Camera.h
    ${RENDERER}/include/CubeMap.h
    ${RENDERER}/include/Framebuffer.h
    ${RENDERER}/include/Materials/Material.h
    ${RENDERER}/include/Materials/UnlitMaterial.h
    ${RENDERER}/include/OpenGLObject.h
    ${RENDERER}/include/Primatives/Cube.h
    ${RENDERER}/include/Primatives/Entity.h
    ${RENDERER}/include/Primatives/FullScreenQuad.h
    ${RENDERER}/include/Primatives/Mesh.h
    ${RENDERER}/include/Primatives/Node.h
    ${RENDERER}/include/Primatives/Plane.h
    ${RENDERER}/include/Primatives/Sphere.h
    ${RENDERER}/include/RenderTargets/RenderTarget.h
    ${RENDERER}/include/RenderTargets/RenderTargetBase.h
    ${RENDERER}/include/Renderbuffer.h
    ${RENDERER}/include/Scene.h
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
    ${RENDERER}/src/Camera.cpp
    ${RENDERER}/src/CubeMap.cpp
    ${RENDERER}/src/Materials/UnlitMaterial.cpp
    ${RENDERER}/src/Primatives/Entity.cpp
    ${RENDERER}/src/Primatives/Mesh.cpp
    ${RENDERER}/src/Primatives/Node.cpp
    ${RENDERER}/src/Scene.cpp
    ${RENDERER}/src/Shaders/ComputeShader.cpp
    ${RENDERER}/src/Shaders/Shader.cpp
    ${RENDERER}/src/Texture.cpp
    ${RENDERER}/src/Utils/FileIO.cpp
)

file(GLOB_RECURSE PRIVATE_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
file(GLOB_RECURSE SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(${TARGET} SHARED ${SRCS} ${PRIVATE_HDRS} ${RENDERER_SRCS} ${RENDERER_HDRS})
target_include_directories(${TARGET} PRIVATE
    # In this project
    ${CMAKE_CURRENT_SOURCE_DIR}/include

    # Renderer related includes
    ${RENDERER}/include
    ${EXTERNAL}
    ${CMAKE_BINARY_DIR}/shaders

    # From OpenXR repo
    "${openxr_SOURCE_DIR}/src/common"
    "${openxr_SOURCE_DIR}/external/include"
)

add_dependencies(${TARGET} shaders)

# export ANativeActivity_onCreate for java to call.
set_property(
    TARGET ${TARGET}
    APPEND_STRING
    PROPERTY LINK_FLAGS " -u ANativeActivity_onCreate"
)

# native_app_glue
include(AndroidNdkModules)
android_ndk_import_module_native_app_glue()
set(RENDERER_LIBS ${RENDERER_LIBS} android native_app_glue)

target_link_libraries(${TARGET} ${RENDERER_LIBS})
target_compile_options(${TARGET} PRIVATE -Wno-cast-calling-convention)
AddGraphicsAPIDefine(${TARGET})
target_compile_definitions(${TARGET} PUBLIC XR_TUTORIAL_ENABLE_MULTIVIEW)

include(${CMAKE_SOURCE_DIR}/cmake/gfxwrapper.cmake)
if(TARGET openxr-gfxwrapper)
    target_include_directories(${TARGET} PUBLIC ${openxr_SOURCE_DIR}/src/common)
    target_link_libraries(${TARGET} openxr-gfxwrapper)
    target_compile_definitions(${TARGET} PUBLIC XR_TUTORIAL_USE_OPENGL_ES)
endif()
