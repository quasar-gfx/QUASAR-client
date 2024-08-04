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

add_subdirectory(${EXTERNAL}/stb/tnt)
set(RENDERER_LIBS ${RENDERER_LIBS} stb)

add_subdirectory(${SHADERS})

# Files
set(RENDERER_INCLUDES
    ${RENDERER}/include/OpenGLObject.h
    ${RENDERER}/include/Texture.h
    ${RENDERER}/include/Shaders/ShaderBase.h
    ${RENDERER}/include/Shaders/Shader.h
    ${RENDERER}/include/Shaders/ComputeShader.h
    ${RENDERER}/include/Utils/FileIO.h
)
set(RENDERER_SRCS
    ${RENDERER}/src/Texture.cpp
    ${RENDERER}/src/Shaders/Shader.cpp
    ${RENDERER}/src/Shaders/ComputeShader.cpp
    ${RENDERER}/src/Utils/FileIO.cpp
)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Utils/HelperFunctions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DebugOutput.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/GraphicsAPI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/GraphicsAPI_OpenGL_ES.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/OpenXRDebugUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/OpenXRHelper.h)
set(SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GraphicsAPI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GraphicsAPI_OpenGL_ES.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OpenXRDebugUtils.cpp)

add_library(${TARGET} SHARED ${SRCS} ${INCLUDES} ${RENDERER_SRCS} ${RENDERER_INCLUDES})
target_include_directories(${TARGET} PRIVATE
    # In this repo
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${RENDERER}/include
    ${EXTERNAL}
    # From OpenXR repo
    "${openxr_SOURCE_DIR}/src/common"
    "${openxr_SOURCE_DIR}/external/include"
)

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
