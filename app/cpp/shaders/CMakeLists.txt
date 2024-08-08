cmake_minimum_required(VERSION 3.22)
project(shaders)

file(GLOB_RECURSE GLSL_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.geom"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.comp"
)

set(OUTPUT_DIR ${CMAKE_SOURCE_DIR}/assets/shaders)

# make assets/shaders directory
file(MAKE_DIRECTORY ${OUTPUT_DIR})

# create link of every file to assets/shaders directory
foreach(GLSL_SRC ${GLSL_SRCS})
    get_filename_component(FILE_NAME ${GLSL_SRC} NAME)
    file(RELATIVE_PATH REL_GLSL_SRC ${OUTPUT_DIR} ${GLSL_SRC})
    get_filename_component(DIR_NAME ${GLSL_SRC} DIRECTORY)
    file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${DIR_NAME})
    file(MAKE_DIRECTORY ${OUTPUT_DIR}/${REL_PATH})
    file(CREATE_LINK ${REL_GLSL_SRC} ${OUTPUT_DIR}/${REL_PATH}${FILE_NAME} SYMBOLIC)
endforeach(GLSL_SRC)
