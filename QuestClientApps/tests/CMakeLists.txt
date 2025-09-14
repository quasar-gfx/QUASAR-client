cmake_minimum_required(VERSION 3.22)
project(AndroidDebugOutputTests LANGUAGES CXX)

enable_testing()
add_executable(debug_output_test debug_output_test.cpp)
target_compile_features(debug_output_test PRIVATE cxx_std_20)
target_include_directories(debug_output_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Libs/include"
)

foreach(test IN ITEMS long_line multiline flush teardown)
    add_test(NAME debug_output_${test} COMMAND debug_output_test ${test})
endforeach()

find_path(OPENXR_INCLUDE_DIR openxr/openxr.h)
if(NOT EXISTS "${OPENXR_INCLUDE_DIR}/openxr/openxr.h")
    message(FATAL_ERROR "Set OPENXR_INCLUDE_DIR to the OpenXR SDK include directory")
endif()

add_executable(foveation_test foveation_test.cpp)
target_compile_features(foveation_test PRIVATE cxx_std_20)
target_include_directories(foveation_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Libs/include"
    "${OPENXR_INCLUDE_DIR}"
)
foreach(test IN ITEMS extensions partial_extensions lookup_failure create_failure update_failure success)
    add_test(NAME foveation_${test} COMMAND foveation_test ${test})
endforeach()
