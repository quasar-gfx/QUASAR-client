# Copyright 2023, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

include_guard()

set(XR_TUTORIAL_GRAPHICS_API "OPENGL_ES" CACHE STRING "Which graphics API to use when building the tutorial projects.")

function(AddGraphicsAPIDefine PROJECT_NAME)
    target_compile_definitions(${PROJECT_NAME} PUBLIC XR_TUTORIAL_GRAPHICS_API=${XR_TUTORIAL_GRAPHICS_API})
endfunction(AddGraphicsAPIDefine)
