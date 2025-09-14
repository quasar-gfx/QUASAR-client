#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <Utils/OpenXRFoveation.h>

static std::string scenario;
static int created = 0, updated = 0, destroyed = 0;
static XrSession session = reinterpret_cast<XrSession>(1);
static XrSwapchain swapchain = reinterpret_cast<XrSwapchain>(2);
static XrFoveationProfileFB profile = reinterpret_cast<XrFoveationProfileFB>(3);

static void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static XrResult XRAPI_CALL createProfile(XrSession value, const XrFoveationProfileCreateInfoFB* info,
                                       XrFoveationProfileFB* result) {
    ++created;
    expect(value == session, "profile must use the active session");
    const auto* level = static_cast<const XrFoveationLevelProfileCreateInfoFB*>(info->next);
    expect(level && level->type == XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB &&
           level->level == XR_FOVEATION_LEVEL_HIGH_FB && level->verticalOffset == 0.0f &&
           level->dynamic == XR_FOVEATION_DYNAMIC_DISABLED_FB, "preserve high fixed foveation");
    if (scenario == "create_failure") return XR_ERROR_OUT_OF_MEMORY;
    *result = profile;
    return XR_SUCCESS;
}

static XrResult XRAPI_CALL updateSwapchain(XrSwapchain value, const XrSwapchainStateBaseHeaderFB* state) {
    ++updated;
    const auto* foveation = reinterpret_cast<const XrSwapchainStateFoveationFB*>(state);
    expect(value == swapchain && foveation->type == XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB &&
           foveation->profile == profile, "update the color swapchain with the created profile");
    return scenario == "update_failure" ? XR_ERROR_RUNTIME_FAILURE : XR_SUCCESS;
}

static XrResult XRAPI_CALL destroyProfile(XrFoveationProfileFB value) {
    ++destroyed;
    expect(value == profile, "destroy the created profile");
    return XR_SUCCESS;
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(
        XrInstance, const char* name, PFN_xrVoidFunction* function) {
    if (scenario == "lookup_failure") return XR_ERROR_FUNCTION_UNSUPPORTED;
    if (std::strcmp(name, "xrCreateFoveationProfileFB") == 0)
        *function = reinterpret_cast<PFN_xrVoidFunction>(createProfile);
    else if (std::strcmp(name, "xrUpdateSwapchainFB") == 0)
        *function = reinterpret_cast<PFN_xrVoidFunction>(updateSwapchain);
    else if (std::strcmp(name, "xrDestroyFoveationProfileFB") == 0)
        *function = reinterpret_cast<PFN_xrVoidFunction>(destroyProfile);
    else return XR_ERROR_FUNCTION_UNSUPPORTED;
    return XR_SUCCESS;
}

static void testExtensions(bool partial) {
    const char* names[] = {"XR_FB_swapchain_update_state", "XR_FB_swapchain_update_state_opengl_es",
                           "XR_FB_foveation", "XR_FB_foveation_configuration"};
    for (int missing = partial ? 0 : 4; missing <= (partial ? 3 : 4); ++missing) {
        std::vector<XrExtensionProperties> available;
        for (int i = 0; i < 4; ++i) {
            if (i == missing) continue;
            XrExtensionProperties extension{XR_TYPE_EXTENSION_PROPERTIES};
            std::strcpy(extension.extensionName, names[i]);
            available.push_back(extension);
        }
        std::vector<const char*> enabled{"XR_KHR_opengl_es_enable"};
        const bool supported = quasar::EnableFoveationExtensions(available, enabled);
        expect(supported == !partial, "all four extensions are required");
        expect(enabled.size() == (partial ? 1 : 5), "enable all extensions together or leave the list intact");
        if (!partial) {
            for (int i = 0; i < 4; ++i)
                expect(std::strcmp(enabled[i + 1], names[i]) == 0, "enable the required extension names");
        }
    }
}

int main(int argc, char** argv) {
    try {
        expect(argc == 2, "expected a test name");
        scenario = argv[1];
        if (scenario == "extensions" || scenario == "partial_extensions") {
            testExtensions(scenario == "partial_extensions");
        } else {
            const XrResult result = quasar::ApplyFoveation(XR_NULL_HANDLE, session, swapchain);
            if (scenario == "lookup_failure")
                expect(result == XR_ERROR_FUNCTION_UNSUPPORTED && created == 0 && updated == 0 && destroyed == 0,
                       "failed lookup must not call unresolved functions");
            else if (scenario == "create_failure")
                expect(result == XR_ERROR_OUT_OF_MEMORY && created == 1 && updated == 0 && destroyed == 0,
                       "failed creation must not use an uninitialized profile");
            else if (scenario == "update_failure")
                expect(result == XR_ERROR_RUNTIME_FAILURE && created == 1 && updated == 1 && destroyed == 1,
                       "failed updates must still release the profile and return the failure");
            else if (scenario == "success")
                expect(result == XR_SUCCESS && created == 1 && updated == 1 && destroyed == 1,
                       "successful foveation must create, apply, and release one profile");
            else throw std::runtime_error("unknown test");
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
