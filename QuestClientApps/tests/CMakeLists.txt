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
