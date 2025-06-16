cmake_minimum_required(VERSION 3.22)
set(TARGET questclient)
project(${TARGET})

# additional directories for find_package() to search within
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include("${CMAKE_SOURCE_DIR}/cmake/graphics_api_select.cmake")

set(QUASAR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/QUASAR)
set(QUASAR_OUTPUT_DIR ${CMAKE_BINARY_DIR}/Libs/QUASAR)

set(QUASAR_APP_COMMON_DIR ${QUASAR_DIR}/apps/Common)
set(QUASAR_APP_COMMON_SHADERS_DIR ${QUASAR_APP_COMMON_DIR}/shaders)

# include fetchcontent for external dependencies
include(FetchContent)

# openxr_loader - from github.com/KhronosGroup
set(BUILD_TESTS OFF)
set(BUILD_API_LAYERS ON)
FetchContent_Declare(
    OpenXR
    URL_HASH MD5=924a94a2da0b5ef8e82154c623d88644
    URL https://github.com/KhronosGroup/OpenXR-SDK-Source/archive/refs/tags/release-1.0.34.zip
    SOURCE_DIR openxr
)
FetchContent_MakeAvailable(OpenXR)

# ffmpeg
add_subdirectory(ffmpeg-libs)

# QUASAR
set(QUASAR_BUILD_APPS OFF) # disable building apps
add_subdirectory(${QUASAR_DIR})

# quasar_common source files
set(COMMON_SRCS
    ${QUASAR_APP_COMMON_DIR}/src/BC4DepthVideoTexture.cpp
    ${QUASAR_APP_COMMON_DIR}/src/DataReceiverTCP.cpp
    ${QUASAR_APP_COMMON_DIR}/src/DataReceiverUDP.cpp
    ${QUASAR_APP_COMMON_DIR}/src/DataStreamerTCP.cpp
    ${QUASAR_APP_COMMON_DIR}/src/DataStreamerUDP.cpp
    ${QUASAR_APP_COMMON_DIR}/src/Quads/DepthOffsets.cpp
    ${QUASAR_APP_COMMON_DIR}/src/Quads/MeshFromQuads.cpp
    ${QUASAR_APP_COMMON_DIR}/src/Quads/QuadMaterial.cpp
    ${QUASAR_APP_COMMON_DIR}/src/Quads/QuadsBuffers.cpp
    ${QUASAR_APP_COMMON_DIR}/src/VideoTexture.cpp
)

# quasar_common shaders
add_subdirectory(${QUASAR_APP_COMMON_SHADERS_DIR})

# app library source files
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# add android native app glue
add_library(app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

# create the target library
add_library(${TARGET}
    STATIC
    ${SRCS}
    ${COMMON_SRCS}
)

target_compile_options(${TARGET} PRIVATE -Wno-cast-calling-convention -Wunused-variable)

# apply graphics api definitions
AddGraphicsAPIDefine(${TARGET})
target_compile_definitions(${TARGET} PRIVATE QUEST_CLIENT_ENABLE_MULTIVIEW)

# include gfxwrapper if available
include(${CMAKE_SOURCE_DIR}/cmake/gfxwrapper.cmake)
if(TARGET openxr-gfxwrapper)
    target_include_directories(${TARGET} PRIVATE ${openxr_SOURCE_DIR}/src/common)
    target_link_libraries(${TARGET} PRIVATE openxr-gfxwrapper)
    target_compile_definitions(${TARGET} PRIVATE QUEST_CLIENT_USE_OPENGL_ES)
endif()

# link target libraries
target_link_libraries(${TARGET}
    PUBLIC
    android
    app_glue
    openxr_loader
    quasar
    libffmpeg
)

# include directories
target_include_directories(${TARGET}
    PUBLIC
    # android ndk includes
    ${ANDROID_NDK}/sources/android/native_app_glue

    # from openxr repo
    ${openxr_SOURCE_DIR}/src/common
    ${openxr_SOURCE_DIR}/external/include

    # quasar_common includes
    ${QUASAR_APP_COMMON_DIR}/include
    ${QUASAR_OUTPUT_DIR}/apps/Common/shaders

    # from this project
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
