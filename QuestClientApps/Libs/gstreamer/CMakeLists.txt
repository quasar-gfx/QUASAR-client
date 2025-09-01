cmake_minimum_required(VERSION 3.22)
project(libgstreamer)

# Download and Extract GStreamer if not already done
set(GSTREAMER_URL "https://gstreamer.freedesktop.org/data/pkg/android/1.27.1/gstreamer-1.0-android-universal-1.27.1.tar.xz")
set(GSTREAMER_ARCHIVE "${CMAKE_BINARY_DIR}/gstreamer.tar.xz")
set(GSTREAMER_EXPECTED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/arm64-v8a")  # Rename target

if(NOT EXISTS "${GSTREAMER_EXPECTED_DIR}")
    message(STATUS "Downloading GStreamer...")
    file(DOWNLOAD ${GSTREAMER_URL} ${GSTREAMER_ARCHIVE} SHOW_PROGRESS)

    message(STATUS "Extracting GStreamer...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xJf ${GSTREAMER_ARCHIVE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE TAR_RESULT
    )
    if(NOT TAR_RESULT EQUAL 0)
        message(FATAL_ERROR "Extraction failed with code: ${TAR_RESULT}")
    endif()

    # Rename arm64 to arm64-v8a to match ABI name
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/arm64")
        file(RENAME "${CMAKE_CURRENT_SOURCE_DIR}/arm64" "${CMAKE_CURRENT_SOURCE_DIR}/arm64-v8a")
        message(STATUS "Renamed arm64 -> arm64-v8a")
    endif()

    # Rename armv7 to armeabi-v7a to match ABI name
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/armv7")
        file(RENAME "${CMAKE_CURRENT_SOURCE_DIR}/armv7" "${CMAKE_CURRENT_SOURCE_DIR}/armeabi-v7a")
        message(STATUS "Renamed armv7 -> armeabi-v7a")
    endif()
endif()

# Add GStreamer libraries
set(GSTREAMER_ABI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${ANDROID_ABI}")
set(GSTREAMER_LIBS_DIR "${GSTREAMER_ABI_DIR}/lib")
set(GSTREAMER_PLUGIN_DIR "${GSTREAMER_LIBS_DIR}/gstreamer-1.0")
set(GSTREAMER_INCLUDE_DIR "${GSTREAMER_ABI_DIR}/include")

set(GSTREAMER_LIB_NAMES
    gstreamer-1.0
    gstbase-1.0
    gstapp-1.0
    gstvideo-1.0
    gstcodecparsers-1.0
    gstnet-1.0
    gstpbutils-1.0
    gsttag-1.0
    gstgl-1.0
    gstaudio-1.0
    gstphotography-1.0
    gobject-2.0
    glib-2.0
    gmodule-2.0
    gio-2.0
    gstrtp-1.0
    orc-0.4
    pcre2-8
    intl
    iconv
    ffi
)

set(GSTREAMER_PLUGIN_LIB_NAMES
    coreelements
    app
    gio
    videoconvertscale
    videorate
    autodetect
    rtp
    rtpmanager
    udp
    playback
    androidmedia
    videoparsersbad
)

set(GSTREAMER_STATIC_LIBS "")
foreach(LIB ${GSTREAMER_LIB_NAMES})
    list(APPEND GSTREAMER_STATIC_LIBS "${GSTREAMER_LIBS_DIR}/lib${LIB}.a")
endforeach()

foreach(PLUGIN ${GSTREAMER_PLUGIN_LIB_NAMES})
    list(APPEND GSTREAMER_STATIC_LIBS "${GSTREAMER_PLUGIN_DIR}/libgst${PLUGIN}.a")
endforeach()

add_library(libgstreamer INTERFACE)

target_link_libraries(libgstreamer INTERFACE ${GSTREAMER_STATIC_LIBS})

target_include_directories(libgstreamer INTERFACE
    "${GSTREAMER_INCLUDE_DIR}/gstreamer-1.0"
    "${GSTREAMER_INCLUDE_DIR}/glib-2.0"
    "${GSTREAMER_LIBS_DIR}/glib-2.0/include"
    "${GSTREAMER_LIBS_DIR}/gstreamer-1.0/include"
    "${GSTREAMER_INCLUDE_DIR}"
)
