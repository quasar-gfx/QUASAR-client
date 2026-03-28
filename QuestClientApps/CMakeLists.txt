cmake_minimum_required(VERSION 3.22)
project(QuestClientApps)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

find_program(OPENXR_PYTHON NAMES python3 python)
foreach(CANDIDATE IN ITEMS "${PYTHON_EXECUTABLE}" "${OPENXR_PYTHON}" /usr/bin/python3)
    if(EXISTS "${CANDIDATE}")
        execute_process(COMMAND "${CANDIDATE}" -c "import markupsafe"
                        RESULT_VARIABLE OPENXR_MARKUPSAFE OUTPUT_QUIET ERROR_QUIET)
        if(NOT OPENXR_MARKUPSAFE EQUAL 0)
            set(PYTHON_EXECUTABLE "${CANDIDATE}" CACHE FILEPATH "Python for OpenXR codegen" FORCE)
            break()
        endif()
    endif()
endforeach()

# add libraries
add_subdirectory(Libs)

# add our code
add_subdirectory(Apps)
