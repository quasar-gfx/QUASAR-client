cmake_minimum_required(VERSION 3.22)
project(shaders)

file(GLOB_RECURSE SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.geom"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.comp"
)

# make assets/shaders directory
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/assets/shaders)

# create link of every file to assets/shaders directory
foreach(GLSL_SRC ${SRCS})
    get_filename_component(FILE_NAME ${GLSL_SRC} NAME_WE)
    get_filename_component(FILE_EXT ${GLSL_SRC} LAST_EXT)
    file(CREATE_LINK ${GLSL_SRC} ${CMAKE_SOURCE_DIR}/assets/shaders/${FILE_NAME}${FILE_EXT} SYMBOLIC)
endforeach(GLSL_SRC)
