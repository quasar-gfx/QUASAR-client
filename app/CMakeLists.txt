# Copyright 2023, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22.1)
set(PROJECT_NAME OculusClient)
project("${PROJECT_NAME}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

add_compile_options(-Wunused-variable)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

# Additional Directories for find_package() to search within.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include("cmake/graphics_api_select.cmake")

message("Using graphics API: " ${XR_TUTORIAL_GRAPHICS_API})

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

# Files
set(SOURCES
    cpp/main.cpp
    cpp/common/GraphicsAPI.cpp
    cpp/common/GraphicsAPI_OpenGL_ES.cpp
    cpp/common/OpenXRDebugUtils.cpp)
set(HEADERS
    cpp/common/DebugOutput.h
    cpp/common/GraphicsAPI.h
    cpp/common/GraphicsAPI_OpenGL_ES.h
    cpp/common/HelperFunctions.h
    cpp/common/OpenXRDebugUtils.h
    cpp/common/OpenXRHelper.h)

set(ES_GLSL_SHADERS
    cpp/shaders/VertexShader_GLES_MV.glsl
    cpp/shaders/PixelShader_GLES_MV.glsl)

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})
target_include_directories(${PROJECT_NAME} PRIVATE
    # In this repo
    cpp/common/
    # From OpenXR repo
    "${openxr_SOURCE_DIR}/src/common"
    "${openxr_SOURCE_DIR}/external/include"
)

# export ANativeActivity_onCreate for java to call.
set_property(
    TARGET ${PROJECT_NAME}
    APPEND_STRING
    PROPERTY LINK_FLAGS " -u ANativeActivity_onCreate"
)

# native_app_glue
include(AndroidNdkModules)
android_ndk_import_module_native_app_glue()

target_link_libraries(${PROJECT_NAME} android native_app_glue openxr_loader)
target_compile_options(${PROJECT_NAME} PRIVATE -Wno-cast-calling-convention)
AddGraphicsAPIDefine(${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME} PUBLIC XR_TUTORIAL_ENABLE_MULTIVIEW)

include(cmake/gfxwrapper.cmake)
if(TARGET openxr-gfxwrapper)
    target_include_directories(${PROJECT_NAME} PUBLIC ${openxr_SOURCE_DIR}/src/common)
    target_link_libraries(${PROJECT_NAME} openxr-gfxwrapper)
    target_compile_definitions(${PROJECT_NAME} PUBLIC XR_TUTORIAL_USE_OPENGL_ES)
endif()

# OpenGL ES GLSL
set(SHADER_DEST "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")
foreach(FILE ${ES_GLSL_SHADERS})
    get_filename_component(FILE_WE ${FILE} NAME_WE)
    add_custom_command(
        OUTPUT "${SHADER_DEST}/${FILE_WE}.glsl"
        COMMAND
            ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
            "${SHADER_DEST}/${FILE_WE}.glsl"
        COMMENT "GLSL ${FILE}"
        MAIN_DEPENDENCY "${FILE}"
        DEPEND "${FILE}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM
    )
    # Make our project depend on these files
    target_sources(${PROJECT_NAME} PRIVATE "${SHADER_DEST}/${FILE_WE}.glsl")
endforeach(FILE)
