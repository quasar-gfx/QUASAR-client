cmake_minimum_required(VERSION 3.22)
project(shaders)

set(OUTPUT_DIR ${CMAKE_SOURCE_DIR}/assets/shaders)

file(GLOB_RECURSE GLSL_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.geom"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.comp"
)

# make assets/shaders directory
file(MAKE_DIRECTORY ${OUTPUT_DIR})

# create link of every file to assets/shaders directory
foreach(GLSL_SRC ${GLSL_SRCS})
    get_filename_component(FILE_NAME ${GLSL_SRC} NAME)
    file(CREATE_LINK ${GLSL_SRC} ${OUTPUT_DIR}/${FILE_NAME} SYMBOLIC)
endforeach(GLSL_SRC)
