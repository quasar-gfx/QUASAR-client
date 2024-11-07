cmake_minimum_required(VERSION 3.22)
set(TARGET QuestClient)
project(${TARGET})

# our code
file(GLOB_RECURSE SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(${TARGET} SHARED ${SRCS} ${RENDERER_SRCS} ${COMMON_SRCS})
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
    ${FFMPEG_INCLUDE_DIR}/${ANDROID_ABI}

    # From OpenXR repo
    ${openxr_SOURCE_DIR}/src/common
    ${openxr_SOURCE_DIR}/external/include
)

add_dependencies(${TARGET} shaders_builtin shaders_common)

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
