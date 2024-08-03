cmake_minimum_required(VERSION 3.22)
project(cpp)

set(RENDERER ${CMAKE_CURRENT_SOURCE_DIR}/renderer)
set(SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(EXTERNAL ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# Additional Directories for find_package() to search within.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include("${CMAKE_SOURCE_DIR}/cmake/graphics_api_select.cmake")

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
set(SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/GraphicsAPI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/GraphicsAPI_OpenGL_ES.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/OpenXRDebugUtils.cpp)
set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/common/DebugOutput.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/GraphicsAPI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/GraphicsAPI_OpenGL_ES.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/HelperFunctions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/OpenXRDebugUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/OpenXRHelper.h)

add_library(${TARGET} SHARED ${SRCS} ${INCLUDES})
target_include_directories(${TARGET} PRIVATE
    # In this repo
    ${CMAKE_CURRENT_SOURCE_DIR}/common/
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

target_link_libraries(${TARGET} android native_app_glue openxr_loader)
target_compile_options(${TARGET} PRIVATE -Wno-cast-calling-convention)
AddGraphicsAPIDefine(${TARGET})
target_compile_definitions(${TARGET} PUBLIC XR_TUTORIAL_ENABLE_MULTIVIEW)

include(${CMAKE_SOURCE_DIR}/cmake/gfxwrapper.cmake)
if(TARGET openxr-gfxwrapper)
    target_include_directories(${TARGET} PUBLIC ${openxr_SOURCE_DIR}/src/common)
    target_link_libraries(${TARGET} openxr-gfxwrapper)
    target_compile_definitions(${TARGET} PUBLIC XR_TUTORIAL_USE_OPENGL_ES)
endif()

add_subdirectory(${SHADERS})
