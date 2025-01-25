cmake_minimum_required(VERSION 3.22)
set(TARGET LibFFmpeg)
project(${TARGET})

# Add ffmpeg libraries
set(FFMPEG_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(FFMPEG_INCLUDE_DIR ${FFMPEG_ROOT_DIR}/include)
set(FFMPEG_LIBS_DIR ${FFMPEG_ROOT_DIR}/lib/${ANDROID_ABI})
set(FFMPEG_LIBS avutil avformat avcodec swscale swresample)

add_library(${TARGET} INTERFACE)

foreach(LIB_NAME ${FFMPEG_LIBS})
    add_library(${LIB_NAME} SHARED IMPORTED)
    set_target_properties(${LIB_NAME} PROPERTIES IMPORTED_LOCATION ${FFMPEG_LIBS_DIR}/lib${LIB_NAME}.so)
    target_link_libraries(${TARGET} INTERFACE ${LIB_NAME})
endforeach()

message(STATUS "FFMPEG_INCLUDE_DIR: ${FFMPEG_INCLUDE_DIR}/${ANDROID_ABI}")
target_include_directories(${TARGET} INTERFACE ${FFMPEG_INCLUDE_DIR}/${ANDROID_ABI})
