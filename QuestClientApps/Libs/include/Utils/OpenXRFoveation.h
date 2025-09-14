#pragma once

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#include <openxr/openxr.h>

namespace quasar {

inline bool EnableFoveationExtensions(const std::vector<XrExtensionProperties>& available,
                                     std::vector<const char*>& enabled) {
    constexpr std::array names{
        XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME,
        "XR_FB_swapchain_update_state_opengl_es",
        XR_FB_FOVEATION_EXTENSION_NAME,
        XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME,
    };
    for (const char* name : names) {
        if (std::none_of(available.begin(), available.end(), [name](const auto& extension) {
                return std::strcmp(extension.extensionName, name) == 0;
            })) {
            return false;
        }
    }
    enabled.insert(enabled.end(), names.begin(), names.end());
    return true;
}

inline XrResult ApplyFoveation(XrInstance instance, XrSession session, XrSwapchain swapchain) {
    auto loadFunction = [instance](const char* name, auto& function) {
        XrResult result = xrGetInstanceProcAddr(instance, name, reinterpret_cast<PFN_xrVoidFunction*>(&function));
        if (XR_FAILED(result)) return result;
        return function ? XR_SUCCESS : XR_ERROR_FUNCTION_UNSUPPORTED;
    };
    PFN_xrCreateFoveationProfileFB createProfile = nullptr;
    PFN_xrDestroyFoveationProfileFB destroyProfile = nullptr;
    PFN_xrUpdateSwapchainFB updateSwapchain = nullptr;
    XrResult result = loadFunction("xrCreateFoveationProfileFB", createProfile);
    if (XR_FAILED(result)) return result;
    result = loadFunction("xrDestroyFoveationProfileFB", destroyProfile);
    if (XR_FAILED(result)) return result;
    result = loadFunction("xrUpdateSwapchainFB", updateSwapchain);
    if (XR_FAILED(result)) return result;

    XrFoveationLevelProfileCreateInfoFB level{XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB};
    level.level = XR_FOVEATION_LEVEL_HIGH_FB;
    level.dynamic = XR_FOVEATION_DYNAMIC_DISABLED_FB;
    XrFoveationProfileCreateInfoFB profileInfo{XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB};
    profileInfo.next = &level;
    XrFoveationProfileFB profile = XR_NULL_HANDLE;
    result = createProfile(session, &profileInfo, &profile);
    if (XR_FAILED(result)) return result;

    XrSwapchainStateFoveationFB state{XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB};
    state.profile = profile;
    result = updateSwapchain(swapchain, reinterpret_cast<const XrSwapchainStateBaseHeaderFB*>(&state));
    XrResult destroyResult = destroyProfile(profile);
    return XR_FAILED(result) ? result : destroyResult;
}

} // namespace quasar
