cmake_minimum_required(VERSION 3.22)
set(TARGET SceneViewer)
project(${TARGET})

set(APP_LIB questclient)

# our code
file(GLOB_RECURSE SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(${TARGET} SHARED ${SRCS})
target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_dependencies(${TARGET} shaders_builtin shaders_common)

# export ANativeActivity_onCreate for java to call.
set_property(
    TARGET ${TARGET}
    APPEND_STRING
    PROPERTY LINK_FLAGS " -u ANativeActivity_onCreate"
)

target_link_libraries(${TARGET} ${APP_LIB})
