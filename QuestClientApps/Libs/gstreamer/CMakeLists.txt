cmake_minimum_required(VERSION 3.22)
set(TARGET libgstreamer)
project(${TARGET})

# ABI paths
set(GSTREAMER_ABI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${ANDROID_ABI})
set(GSTREAMER_INCLUDE_DIR ${GSTREAMER_ABI_DIR}/include)
set(GSTREAMER_LIBS_DIR ${GSTREAMER_ABI_DIR}/lib)
set(GSTREAMER_PLUGIN_DIR ${GSTREAMER_LIBS_DIR}/gstreamer-1.0)

# Core libraries
set(GSTREAMER_LIB_NAMES
    gstreamer-1.0
    gstnet-1.0
    gstapp-1.0
    gstpbutils-1.0
    gsttag-1.0
    gstgl-1.0
    gstaudio-1.0
    gstphotography-1.0
    orc-0.4
    gobject-2.0
    glib-2.0
    gmodule-2.0
    gio-2.0
    gstbase-1.0
    gstapp-1.0
    gstvideo-1.0
    gstrtp-1.0
    pcre2-8
    intl
    iconv
    ffi
)

# Plugin libraries
set(GSTREAMER_PLUGIN_LIB_NAMES
    gstcoreelements
    gstudp
    gstapp
    # gstandroidmedia
    gstrtp
    gstlibav
    gstrtpmanager
    gstvideoconvertscale
    gstlibav
    gstplayback
)

set(GSTREAMER_STATIC_LIBS "")
foreach(LIB ${GSTREAMER_LIB_NAMES})
    list(APPEND GSTREAMER_STATIC_LIBS ${GSTREAMER_LIBS_DIR}/lib${LIB}.a)
endforeach()

foreach(PLUGIN ${GSTREAMER_PLUGIN_LIB_NAMES})
    list(APPEND GSTREAMER_STATIC_LIBS ${GSTREAMER_PLUGIN_DIR}/lib${PLUGIN}.a)
endforeach()

# Interface target
add_library(${TARGET} INTERFACE)

# Link static libs
target_link_libraries(${TARGET} INTERFACE ${GSTREAMER_STATIC_LIBS})

# Include directories
target_include_directories(${TARGET} INTERFACE
    ${GSTREAMER_INCLUDE_DIR}/gstreamer-1.0
    ${GSTREAMER_INCLUDE_DIR}/glib-2.0
    ${GSTREAMER_LIBS_DIR}/glib-2.0/include
)
