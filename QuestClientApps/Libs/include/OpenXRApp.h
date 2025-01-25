#ifndef OPENXR_APP_H
#define OPENXR_APP_H

#include <map>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/android_sink.h>

#include <OpenGLAppConfig.h>
#include <OpenGLESRenderer.h>

#include <Scene.h>
#include <Cameras/VRCamera.h>
#include <Utils/FileIO.h>

#include <Utils/DebugOutput.h>
#include <Utils/OpenXRDebugUtils.h>
#include <Utils/GLM_XR_Interop.h>

class OpenXRApp {
protected:
    struct RenderLayerInfo;

public:
    OpenXRApp(GraphicsAPI_Type apiType) : m_apiType(apiType) {
        // Check API compatibility with Platform.
        if (!CheckGraphicsAPI_TypeIsValidForPlatform(m_apiType)) {
            XR_LOG_ERROR("ERROR: The provided Graphics API is not valid for this platform.");
            DEBUG_BREAK;
        }

        // Set up spd for android logging.
        spdlog::set_pattern("%v");
        std::string tag = "spdlog-android";
        auto logger = spdlog::android_logger_mt("android", tag);
        spdlog::set_default_logger(logger);
    }
    ~OpenXRApp() = default;

    void Run() {
        CreateInstance();
        CreateDebugMessenger();

        GetInstanceProperties();
        GetSystemID();
        CreateActionSetInternal();
        SuggestBindingsInternal();
        GetViewConfigurationViews();
        GetEnvironmentBlendModes();
        CreateSession();

        CreateActionPoses();
        AttachActionSet();

        CreateReferenceSpace();
        CreateSwapchains();

        CreateResourcesInternal();

        while (m_applicationRunning) {
            PollSystemEvents();
            PollEvents();
            if (m_sessionRunning) {
                RenderFrame();
            }
        }

        DestroySwapchains();
        DestroyReferenceSpace();
        DestroyResources();
        DestroySession();

        DestroyDebugMessenger();
        DestroyInstance();
    }

protected:
    void CreateInstance() {
        // Fill out an XrApplicationInfo structure detailing the names and OpenXR version.
        // The application/engine name and version are user-definied. These may help IHVs or runtimes.
        XrApplicationInfo AI;
        strncpy(AI.applicationName, "QuestClient", XR_MAX_APPLICATION_NAME_SIZE);
        AI.applicationVersion = 1;
        strncpy(AI.engineName, "OpenXR Engine", XR_MAX_ENGINE_NAME_SIZE);
        AI.engineVersion = 1;
        AI.apiVersion = XR_CURRENT_API_VERSION;

        // Add additional instance layers/extensions that the application wants.
        // Add both required and requested instance extensions.
        {
            // Ensure m_apiType is already defined when we call this line.
            m_instanceExtensions.push_back(GetGraphicsAPIInstanceExtensionString(m_apiType));
            m_instanceExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        // Get all the API Layers from the OpenXR runtime.
        uint32_t apiLayerCount = 0;
        std::vector<XrApiLayerProperties> apiLayerProperties;
        OPENXR_CHECK(xrEnumerateApiLayerProperties(0, &apiLayerCount, nullptr), "Failed to enumerate ApiLayerProperties.");
        apiLayerProperties.resize(apiLayerCount, {XR_TYPE_API_LAYER_PROPERTIES});
        OPENXR_CHECK(xrEnumerateApiLayerProperties(apiLayerCount, &apiLayerCount, apiLayerProperties.data()), "Failed to enumerate ApiLayerProperties.");

        // Check the requested API layers against the ones from the OpenXR. If found add it to the Active API Layers.
        for (auto &requestLayer : m_apiLayers) {
            for (auto &layerProperty : apiLayerProperties) {
                // strcmp returns 0 if the strings match.
                if (strcmp(requestLayer.c_str(), layerProperty.layerName) != 0) {
                    continue;
                }
                else {
                    m_activeAPILayers.push_back(requestLayer.c_str());
                    break;
                }
            }
        }

        // Get all the Instance Extensions from the OpenXR instance.
        uint32_t extensionCount = 0;
        std::vector<XrExtensionProperties> extensionProperties;
        OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr), "Failed to enumerate InstanceExtensionProperties.");
        extensionProperties.resize(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
        OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensionProperties.data()), "Failed to enumerate InstanceExtensionProperties.");

        // Check the requested Instance Extensions against the ones from the OpenXR runtime.
        // If an extension is found add it to Active Instance Extensions.
        // Log error if the Instance Extension is not found.
        for (auto &requestedInstanceExtension : m_instanceExtensions) {
            bool found = false;
            for (auto &extensionProperty : extensionProperties) {
                // strcmp returns 0 if the strings match.
                if (strcmp(requestedInstanceExtension.c_str(), extensionProperty.extensionName) != 0) {
                    continue;
                }
                else {
                    m_activeInstanceExtensions.push_back(requestedInstanceExtension.c_str());
                    found = true;
                    break;
                }
            }
            if (!found) {
                XR_LOG_ERROR("Failed to find OpenXR instance extension: " << requestedInstanceExtension);
            }
        }

        // Fill out an XrInstanceCreateInfo structure and create an XrInstance.
        XrInstanceCreateInfo instanceCI{XR_TYPE_INSTANCE_CREATE_INFO};
        instanceCI.createFlags = 0;
        instanceCI.applicationInfo = AI;
        instanceCI.enabledApiLayerCount = static_cast<uint32_t>(m_activeAPILayers.size());
        instanceCI.enabledApiLayerNames = m_activeAPILayers.data();
        instanceCI.enabledExtensionCount = static_cast<uint32_t>(m_activeInstanceExtensions.size());
        instanceCI.enabledExtensionNames = m_activeInstanceExtensions.data();
        OPENXR_CHECK(xrCreateInstance(&instanceCI, &m_xrInstance), "Failed to create Instance.");
    }

    void DestroyInstance() {
        // Destroy the XrInstance.
        OPENXR_CHECK(xrDestroyInstance(m_xrInstance), "Failed to destroy Instance.");
    }

    void CreateDebugMessenger() {
        // Check that "XR_EXT_debug_utils" is in the active Instance Extensions before creating an XrDebugUtilsMessengerEXT.
        if (IsStringInVector(m_activeInstanceExtensions, XR_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            m_debugUtilsMessenger = CreateOpenXRDebugUtilsMessenger(m_xrInstance);  // From OpenXRDebugUtils.h.
        }
    }

    void DestroyDebugMessenger() {
        // Check that "XR_EXT_debug_utils" is in the active Instance Extensions before destroying the XrDebugUtilsMessengerEXT.
        if (m_debugUtilsMessenger != XR_NULL_HANDLE) {
            DestroyOpenXRDebugUtilsMessenger(m_xrInstance, m_debugUtilsMessenger);  // From OpenXRDebugUtils.h.
        }
    }

    void GetInstanceProperties() {
        // Get the instance's properties and log the runtime name and version.
        XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
        OPENXR_CHECK(xrGetInstanceProperties(m_xrInstance, &instanceProperties), "Failed to get InstanceProperties.");

        XR_LOG("OpenXR Runtime: " << instanceProperties.runtimeName << " - "
                                  << XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << "."
                                  << XR_VERSION_MINOR(instanceProperties.runtimeVersion) << "."
                                  << XR_VERSION_PATCH(instanceProperties.runtimeVersion));
    }

    void GetSystemID() {
        // Get the XrSystemId from the instance and the supplied XrFormFactor.
        XrSystemGetInfo systemGI{XR_TYPE_SYSTEM_GET_INFO};
        systemGI.formFactor = m_formFactor;
        OPENXR_CHECK(xrGetSystem(m_xrInstance, &systemGI, &m_systemID), "Failed to get SystemID.");

        // Get the System's properties for some general information about the hardware and the vendor.
        OPENXR_CHECK(xrGetSystemProperties(m_xrInstance, m_systemID, &m_systemProperties), "Failed to get SystemProperties.");
    }

    XrPath CreateXrPath(const char* path_string) {
        XrPath xrPath;
        OPENXR_CHECK(xrStringToPath(m_xrInstance, path_string, &xrPath), "Failed to create XrPath from string.");
        return xrPath;
    }
    std::string FromXrPath(XrPath path) {
        uint32_t strl;
        char text[XR_MAX_PATH_LENGTH];
        XrResult res;
        res = xrPathToString(m_xrInstance, path, XR_MAX_PATH_LENGTH, &strl, text);
        std::string str;
        if (res == XR_SUCCESS) {
            str = text;
        }
        else {
            OPENXR_CHECK(res, "Failed to retrieve path.");
        }
        return str;
    }

    void CreateAction(XrAction &xrAction, const char* name, XrActionType xrActionType, std::vector<const char*> subaction_paths = {}) {
        XrActionCreateInfo actionCI{XR_TYPE_ACTION_CREATE_INFO};
        // The type of action: float input, pose, haptic output etc.
        actionCI.actionType = xrActionType;
        // Subaction paths, e.g. left and right hand. To distinguish the same action performed on different devices.
        std::vector<XrPath> subaction_xrpaths;
        for (auto p : subaction_paths) {
            subaction_xrpaths.push_back(CreateXrPath(p));
        }
        actionCI.countSubactionPaths = (uint32_t)subaction_xrpaths.size();
        actionCI.subactionPaths = subaction_xrpaths.data();
        // The internal name the runtime uses for this Action.
        strncpy(actionCI.actionName, name, XR_MAX_ACTION_NAME_SIZE);
        // Localized names are required so there is a human-readable action name to show the user if they are rebinding the Action in an options screen.
        strncpy(actionCI.localizedActionName, name, XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
        OPENXR_CHECK(xrCreateAction(m_actionSet, &actionCI, &xrAction), "Failed to create Action.");
    };

    virtual void CreateActionSetInternal() {
        XrActionSetCreateInfo actionSetCI{XR_TYPE_ACTION_SET_CREATE_INFO};
        // The internal name the runtime uses for this Action Set.
        strncpy(actionSetCI.actionSetName, "openxr-app-actionset", XR_MAX_ACTION_SET_NAME_SIZE);
        // Localized names are required so there is a human-readable action name to show the user if they are rebinding Actions in an options screen.
        strncpy(actionSetCI.localizedActionSetName, "OpenXR App ActionSet", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
        OPENXR_CHECK(xrCreateActionSet(m_xrInstance, &actionSetCI, &m_actionSet), "Failed to create ActionSet.");
        // Set a priority: this comes into play when we have multiple Action Sets, and determines which Action takes priority in binding to a specific input.
        actionSetCI.priority = 0;

        // An Action for the position of the palm of the user's hand - appropriate for the location of a grabbing Actions.
        CreateAction(m_palmPoseAction, "palm-pose", XR_ACTION_TYPE_POSE_INPUT, {"/user/hand/left", "/user/hand/right"});
        // For later convenience we create the XrPaths for the subaction path names.
        m_handPaths[0] = CreateXrPath("/user/hand/left");
        m_handPaths[1] = CreateXrPath("/user/hand/right");

        CreateActionSet();
    }
    virtual void CreateActionSet() {}

    bool SuggestBindingsForPath(const char* profilePath, std::vector<XrActionSuggestedBinding> bindings) {
        // The application can call xrSuggestInteractionProfileBindings once per interaction profile that it supports.
        XrInteractionProfileSuggestedBinding interactionProfileSuggestedBinding{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        interactionProfileSuggestedBinding.interactionProfile = CreateXrPath(profilePath);
        interactionProfileSuggestedBinding.suggestedBindings = bindings.data();
        interactionProfileSuggestedBinding.countSuggestedBindings = (uint32_t)bindings.size();
        if (xrSuggestInteractionProfileBindings(m_xrInstance, &interactionProfileSuggestedBinding) == XrResult::XR_SUCCESS)
            return true;
        XR_LOG("Failed to suggest bindings with " << profilePath);
        return false;
    }

    virtual void SuggestBindingsInternal() {
        std::map<std::string, std::vector<XrActionSuggestedBinding>> bindings;

        // Each Action here has two paths, one for each SubAction path.
        bindings["/interaction_profiles/khr/simple_controller"] = {{m_palmPoseAction, CreateXrPath("/user/hand/left/input/grip/pose")},
                                                                   {m_palmPoseAction, CreateXrPath("/user/hand/right/input/grip/pose")}};
        // Each Action here has two paths, one for each SubAction path.
        bindings["/interaction_profiles/oculus/touch_controller"] = {{m_palmPoseAction, CreateXrPath("/user/hand/left/input/grip/pose")},
                                                                     {m_palmPoseAction, CreateXrPath("/user/hand/right/input/grip/pose")}};
        SuggestBindings(bindings);

        bool any_ok = false;
        for (auto& [profilePath, bindings] : bindings) {
            any_ok |= SuggestBindingsForPath(profilePath.c_str(), bindings);
        }
        if (!any_ok) {
            DEBUG_BREAK;
        }
    }
    virtual void SuggestBindings(std::map<std::string, std::vector<XrActionSuggestedBinding>>& bindings) {}

    void RecordCurrentBindings() {
        if (m_session) {
            // now we are ready to:
            XrInteractionProfileState interactionProfile = {XR_TYPE_INTERACTION_PROFILE_STATE, 0, 0};
            // for each action, what is the binding?
            OPENXR_CHECK(xrGetCurrentInteractionProfile(m_session, m_handPaths[0], &interactionProfile), "Failed to get profile.");
            if (interactionProfile.interactionProfile) {
                XR_LOG("user/hand/left ActiveProfile " << FromXrPath(interactionProfile.interactionProfile).c_str());
            }
            OPENXR_CHECK(xrGetCurrentInteractionProfile(m_session, m_handPaths[1], &interactionProfile), "Failed to get profile.");
            if (interactionProfile.interactionProfile) {
                XR_LOG("user/hand/right ActiveProfile " << FromXrPath(interactionProfile.interactionProfile).c_str());
            }
        }
    }

    void CreateActionPoses() {
        // Create an xrSpace for a pose action.
        auto CreateActionPoseSpace = [this](XrSession session, XrAction xrAction, const char* subaction_path = nullptr) -> XrSpace {
            XrSpace xrSpace;
            const XrPosef xrPoseIdentity = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
            // Create frame of reference for a pose action
            XrActionSpaceCreateInfo actionSpaceCI{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            actionSpaceCI.action = xrAction;
            actionSpaceCI.poseInActionSpace = xrPoseIdentity;
            if (subaction_path)
                actionSpaceCI.subactionPath = CreateXrPath(subaction_path);
            OPENXR_CHECK(xrCreateActionSpace(session, &actionSpaceCI, &xrSpace), "Failed to create ActionSpace.");
            return xrSpace;
        };
        m_handPoseSpace[0] = CreateActionPoseSpace(m_session, m_palmPoseAction, "/user/hand/left");
        m_handPoseSpace[1] = CreateActionPoseSpace(m_session, m_palmPoseAction, "/user/hand/right");
    }

    void AttachActionSet() {
        // Attach the action set we just made to the session. We could attach multiple action sets!
        XrSessionActionSetsAttachInfo actionSetAttachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        actionSetAttachInfo.countActionSets = 1;
        actionSetAttachInfo.actionSets = &m_actionSet;
        OPENXR_CHECK(xrAttachSessionActionSets(m_session, &actionSetAttachInfo), "Failed to attach ActionSet to Session.");
    }

    void GetEnvironmentBlendModes() {
        // Retrieves the available blend modes. The first call gets the count of the array that will be returned. The next call fills out the array.
        uint32_t environmentBlendModeCount = 0;
        OPENXR_CHECK(xrEnumerateEnvironmentBlendModes(m_xrInstance, m_systemID, m_viewConfiguration, 0, &environmentBlendModeCount, nullptr), "Failed to enumerate EnvironmentBlend Modes.");
        m_environmentBlendModes.resize(environmentBlendModeCount);
        OPENXR_CHECK(xrEnumerateEnvironmentBlendModes(m_xrInstance, m_systemID, m_viewConfiguration, environmentBlendModeCount, &environmentBlendModeCount, m_environmentBlendModes.data()), "Failed to enumerate EnvironmentBlend Modes.");

        // Pick the first application supported blend mode supported by the hardware.
        for (const XrEnvironmentBlendMode &environmentBlendMode : m_applicationEnvironmentBlendModes) {
            if (std::find(m_environmentBlendModes.begin(), m_environmentBlendModes.end(), environmentBlendMode) != m_environmentBlendModes.end()) {
                m_environmentBlendMode = environmentBlendMode;
                break;
            }
        }
        if (m_environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM) {
            XR_LOG_ERROR("Failed to find a compatible blend mode. Defaulting to XR_ENVIRONMENT_BLEND_MODE_OPAQUE.");
            m_environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        }
    }

    void GetViewConfigurationViews() {
        // Gets the View Configuration Types. The first call gets the count of the array that will be returned. The next call fills out the array.
        uint32_t viewConfigurationCount = 0;
        OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance, m_systemID, 0, &viewConfigurationCount, nullptr), "Failed to enumerate View Configurations.");
        m_viewConfigurations.resize(viewConfigurationCount);
        OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance, m_systemID, viewConfigurationCount, &viewConfigurationCount, m_viewConfigurations.data()), "Failed to enumerate View Configurations.");

        // Pick the first application supported View Configuration Type con supported by the hardware.
        for (const XrViewConfigurationType &viewConfiguration : m_applicationViewConfigurations) {
            if (std::find(m_viewConfigurations.begin(), m_viewConfigurations.end(), viewConfiguration) != m_viewConfigurations.end()) {
                m_viewConfiguration = viewConfiguration;
                break;
            }
        }
        if (m_viewConfiguration == XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM) {
            XR_LOG_ERROR("Failed to find a view configuration type. Defaulting to XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO.");
            m_viewConfiguration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        }

        // Gets the View Configuration Views. The first call gets the count of the array that will be returned. The next call fills out the array.
        uint32_t viewConfigurationViewCount = 0;
        OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance, m_systemID, m_viewConfiguration, 0, &viewConfigurationViewCount, nullptr), "Failed to enumerate ViewConfiguration Views.");
        m_viewConfigurationViews.resize(viewConfigurationViewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance, m_systemID, m_viewConfiguration, viewConfigurationViewCount, &viewConfigurationViewCount, m_viewConfigurationViews.data()), "Failed to enumerate ViewConfiguration Views.");
    }

    void CreateSession() {
        // Create an XrSessionCreateInfo structure.
        XrSessionCreateInfo sessionCI{XR_TYPE_SESSION_CREATE_INFO};

        // Create a std::unique_ptr<GraphicsAPI_...> from the instance and system.
        // This call sets up a graphics API that's suitable for use with OpenXR.
        if (m_apiType == OPENGL_ES) {
            m_graphicsAPI = std::make_unique<OpenGLESRenderer>(config, m_xrInstance, m_systemID);
        }
        else {
            XR_LOG_ERROR("ERROR: Unknown Graphics API.");
            DEBUG_BREAK;
        }
        // Fill out the XrSessionCreateInfo structure and create an XrSession.
        //  XR_DOCS_TAG_LBEGIN_CreateSession2
        sessionCI.next = m_graphicsAPI->GetGraphicsBinding();
        sessionCI.createFlags = 0;
        sessionCI.systemId = m_systemID;

        OPENXR_CHECK(xrCreateSession(m_xrInstance, &sessionCI, &m_session), "Failed to create Session.");
    }

    void DestroySession() {
        // Destroy the XrSession.
        OPENXR_CHECK(xrDestroySession(m_session), "Failed to destroy Session.");
    }

    void CreateResourcesInternal() {
        cameras = std::make_unique<VRCamera>();
        scene = std::make_unique<Scene>();
        scene->addChildNode(&m_handNodes[0]);
        scene->addChildNode(&m_handNodes[1]);
        CreateResources();
    }
    virtual void CreateResources() {}

    virtual void DestroyResources() {}

    void PollEvents() {
        // Poll OpenXR for a new event.
        XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
        auto XrPollEvents = [&]() -> bool {
            eventData = {XR_TYPE_EVENT_DATA_BUFFER};
            return xrPollEvent(m_xrInstance, &eventData) == XR_SUCCESS;
        };

        while (XrPollEvents()) {
            switch (eventData.type) {
            // Log the number of lost events from the runtime.
            case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
                XrEventDataEventsLost* eventsLost = reinterpret_cast<XrEventDataEventsLost*>(&eventData);
                XR_LOG("OPENXR: Events Lost: " << eventsLost->lostEventCount);
                break;
            }
            // Log that an instance loss is pending and shutdown the application.
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                XrEventDataInstanceLossPending* instanceLossPending = reinterpret_cast<XrEventDataInstanceLossPending*>(&eventData);
                XR_LOG("OPENXR: Instance Loss Pending at: " << instanceLossPending->lossTime);
                m_sessionRunning = false;
                m_applicationRunning = false;
                break;
            }
            // Log that the interaction profile has changed.
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
                XrEventDataInteractionProfileChanged* interactionProfileChanged = reinterpret_cast<XrEventDataInteractionProfileChanged*>(&eventData);
                XR_LOG("OPENXR: Interaction Profile changed for Session: " << interactionProfileChanged->session);
                if (interactionProfileChanged->session != m_session) {
                    XR_LOG("XrEventDataInteractionProfileChanged for unknown Session");
                    break;
                }
                RecordCurrentBindings();
                break;
            }
            // Log that there's a reference space change pending.
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
                XrEventDataReferenceSpaceChangePending* referenceSpaceChangePending = reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&eventData);
                XR_LOG("OPENXR: Reference Space Change pending for Session: " << referenceSpaceChangePending->session);
                if (referenceSpaceChangePending->session != m_session) {
                   XR_LOG("XrEventDataReferenceSpaceChangePending for unknown Session");
                    break;
                }
                break;
            }
            // Session State changes:
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                XrEventDataSessionStateChanged* sessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
                if (sessionStateChanged->session != m_session) {
                    XR_LOG("XrEventDataSessionStateChanged for unknown Session");
                    break;
                }

                if (sessionStateChanged->state == XR_SESSION_STATE_READY) {
                    // SessionState is ready. Begin the XrSession using the XrViewConfigurationType.
                    XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                    sessionBeginInfo.primaryViewConfigurationType = m_viewConfiguration;
                    OPENXR_CHECK(xrBeginSession(m_session, &sessionBeginInfo), "Failed to begin Session.");
                    m_sessionRunning = true;
                }
                if (sessionStateChanged->state == XR_SESSION_STATE_STOPPING) {
                    // SessionState is stopping. End the XrSession.
                    OPENXR_CHECK(xrEndSession(m_session), "Failed to end Session.");
                    m_sessionRunning = false;
                }
                if (sessionStateChanged->state == XR_SESSION_STATE_EXITING) {
                    // SessionState is exiting. Exit the application.
                    m_sessionRunning = false;
                    m_applicationRunning = false;
                }
                if (sessionStateChanged->state == XR_SESSION_STATE_LOSS_PENDING) {
                    // SessionState is loss pending. Exit the application.
                    // It's possible to try a reestablish an XrInstance and XrSession, but we will simply exit here.
                    m_sessionRunning = false;
                    m_applicationRunning = false;
                }
                // Store state for reference across the application.
                m_sessionState = sessionStateChanged->state;
                break;
            }
            default: {
                break;
            }
            }
        }
    }

    void PollActionsInternal(XrTime predictedTime) {
        // Update our action set with up-to-date input data.
        // First, we specify the actionSet we are polling.
        XrActiveActionSet activeActionSet{};
        activeActionSet.actionSet = m_actionSet;
        activeActionSet.subactionPath = XR_NULL_PATH;

        // Now we sync the Actions to make sure they have current data.
        XrActionsSyncInfo actionsSyncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        actionsSyncInfo.countActiveActionSets = 1;
        actionsSyncInfo.activeActionSets = &activeActionSet;
        OPENXR_CHECK(xrSyncActions(m_session, &actionsSyncInfo), "Failed to sync Actions.");

        XrActionStateGetInfo actionStateGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        // We pose a single Action, twice - once for each subAction Path.
        actionStateGetInfo.action = m_palmPoseAction;
        // For each hand, get the pose state if possible.
        for (int i = 0; i < 2; i++) {
            // Specify the subAction Path.
            actionStateGetInfo.subactionPath = m_handPaths[i];
            OPENXR_CHECK(xrGetActionStatePose(m_session, &actionStateGetInfo, &m_handPoseState[i]), "Failed to get Pose State.");
            if (m_handPoseState[i].isActive) {
                XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
                XrResult res = xrLocateSpace(m_handPoseSpace[i], m_localSpace, predictedTime, &spaceLocation);
                if (XR_UNQUALIFIED_SUCCESS(res) &&
                        (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                        (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
                    gxi::Pose pose = gxi::toGLM(spaceLocation.pose);
                    m_handNodes[i].setPosition(pose.position);
                    m_handNodes[i].setRotationQuat(pose.orientation);
                }
                else {
                    m_handPoseState[i].isActive = false;
                }
            }
        }

        PollActions(predictedTime);
    }

    virtual void PollActions(XrTime predictedTime) {}

    void CreateReferenceSpace() {
        // Fill out an XrReferenceSpaceCreateInfo structure and create a reference XrSpace, specifying a Local space with an identity pose as the origin.
        XrReferenceSpaceCreateInfo referenceSpaceCI{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        referenceSpaceCI.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        referenceSpaceCI.poseInReferenceSpace = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
        OPENXR_CHECK(xrCreateReferenceSpace(m_session, &referenceSpaceCI, &m_localSpace), "Failed to create ReferenceSpace.");
    }

    void DestroyReferenceSpace() {
        // Destroy the reference XrSpace.
        OPENXR_CHECK(xrDestroySpace(m_localSpace), "Failed to destroy Space.")
    }

    void CreateSwapchains() {
        // Get the supported swapchain formats as an array of int64_t and ordered by runtime preference.
        uint32_t formatCount = 0;
        OPENXR_CHECK(xrEnumerateSwapchainFormats(m_session, 0, &formatCount, nullptr), "Failed to enumerate Swapchain Formats");
        std::vector<int64_t> formats(formatCount);
        OPENXR_CHECK(xrEnumerateSwapchainFormats(m_session, formatCount, &formatCount, formats.data()), "Failed to enumerate Swapchain Formats");
        if (m_graphicsAPI->SelectDepthSwapchainFormat(formats) == 0) {
            XR_LOG_ERROR("Failed to find depth format for Swapchain.");
            DEBUG_BREAK;
        }

        bool coherentViews = m_viewConfiguration == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        for (const XrViewConfigurationView &viewConfigurationView : m_viewConfigurationViews) {
            // Check the current view size against the first view.
            coherentViews |= m_viewConfigurationViews[0].recommendedImageRectWidth == viewConfigurationView.recommendedImageRectWidth;
            coherentViews |= m_viewConfigurationViews[0].recommendedImageRectHeight == viewConfigurationView.recommendedImageRectHeight;
        }
        if (!coherentViews) {
            XR_LOG_ERROR("The views are not coherent. Unable to create a single Swapchain.");
            DEBUG_BREAK;
        }
        const XrViewConfigurationView &viewConfigurationView = m_viewConfigurationViews[0];
        uint32_t viewCount = static_cast<uint32_t>(m_viewConfigurationViews.size());

        // Create a color and depth swapchain, and their associated image views.
        // Fill out an XrSwapchainCreateInfo structure and create an XrSwapchain.
        // Color.
        XrSwapchainCreateInfo swapchainCI{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainCI.createFlags = 0;
        swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCI.format = m_graphicsAPI->SelectColorSwapchainFormat(formats);          // Use GraphicsAPI to select the first compatible format.
        swapchainCI.sampleCount = viewConfigurationView.recommendedSwapchainSampleCount;  // Use the recommended values from the XrViewConfigurationView.
        swapchainCI.width = viewConfigurationView.recommendedImageRectWidth;
        swapchainCI.height = viewConfigurationView.recommendedImageRectHeight;
        swapchainCI.faceCount = 1;
        swapchainCI.arraySize = viewCount;
        swapchainCI.mipCount = 1;
        OPENXR_CHECK(xrCreateSwapchain(m_session, &swapchainCI, &m_colorSwapchainInfo.swapchain), "Failed to create Color Swapchain");
        m_colorSwapchainInfo.swapchainFormat = swapchainCI.format;  // Save the swapchain format for later use.

        // Depth.
        swapchainCI.createFlags = 0;
        swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        swapchainCI.format = m_graphicsAPI->SelectDepthSwapchainFormat(formats);          // Use GraphicsAPI to select the first compatible format.
        swapchainCI.sampleCount = viewConfigurationView.recommendedSwapchainSampleCount;  // Use the recommended values from the XrViewConfigurationView.
        swapchainCI.width = viewConfigurationView.recommendedImageRectWidth;
        swapchainCI.height = viewConfigurationView.recommendedImageRectHeight;
        swapchainCI.faceCount = 1;
        swapchainCI.arraySize = viewCount;
        swapchainCI.mipCount = 1;
        OPENXR_CHECK(xrCreateSwapchain(m_session, &swapchainCI, &m_depthSwapchainInfo.swapchain), "Failed to create Depth Swapchain");
        m_depthSwapchainInfo.swapchainFormat = swapchainCI.format;  // Save the swapchain format for later use.

        // XR_DOCS_TAG_BEGIN_EnumerateSwapchainImages
        // Get the number of images in the color/depth swapchain and allocate Swapchain image data via GraphicsAPI to store the returned array.
        uint32_t colorSwapchainImageCount = 0;
        OPENXR_CHECK(xrEnumerateSwapchainImages(m_colorSwapchainInfo.swapchain, 0, &colorSwapchainImageCount, nullptr), "Failed to enumerate Color Swapchain Images.");
        XrSwapchainImageBaseHeader* colorSwapchainImages = m_graphicsAPI->AllocateSwapchainImageData(m_colorSwapchainInfo.swapchain, GraphicsAPI::SwapchainType::COLOR, colorSwapchainImageCount);
        OPENXR_CHECK(xrEnumerateSwapchainImages(m_colorSwapchainInfo.swapchain, colorSwapchainImageCount, &colorSwapchainImageCount, colorSwapchainImages), "Failed to enumerate Color Swapchain Images.");

        uint32_t depthSwapchainImageCount = 0;
        OPENXR_CHECK(xrEnumerateSwapchainImages(m_depthSwapchainInfo.swapchain, 0, &depthSwapchainImageCount, nullptr), "Failed to enumerate Depth Swapchain Images.");
        XrSwapchainImageBaseHeader* depthSwapchainImages = m_graphicsAPI->AllocateSwapchainImageData(m_depthSwapchainInfo.swapchain, GraphicsAPI::SwapchainType::DEPTH, depthSwapchainImageCount);
        OPENXR_CHECK(xrEnumerateSwapchainImages(m_depthSwapchainInfo.swapchain, depthSwapchainImageCount, &depthSwapchainImageCount, depthSwapchainImages), "Failed to enumerate Depth Swapchain Images.");

        // Per image in the swapchains, fill out a GraphicsAPI::ImageViewCreateInfo structure and create a color/depth image view.
        for (uint32_t j = 0; j < colorSwapchainImageCount; j++) {
            GraphicsAPI::ImageViewCreateInfo imageViewCI;
            imageViewCI.image = m_graphicsAPI->GetSwapchainImage(m_colorSwapchainInfo.swapchain, j);
            imageViewCI.type = GraphicsAPI::ImageViewCreateInfo::Type::RTV;
            imageViewCI.view = GraphicsAPI::ImageViewCreateInfo::View::TYPE_2D_ARRAY;
            imageViewCI.format = m_colorSwapchainInfo.swapchainFormat;
            imageViewCI.aspect = GraphicsAPI::ImageViewCreateInfo::Aspect::COLOR_BIT;
            imageViewCI.baseMipLevel = 0;
            imageViewCI.levelCount = 1;
            imageViewCI.baseArrayLayer = 0;
            imageViewCI.layerCount = viewCount;
            m_colorSwapchainInfo.imageViews.push_back(m_graphicsAPI->CreateImageView(imageViewCI));
        }
        for (uint32_t j = 0; j < depthSwapchainImageCount; j++) {
            GraphicsAPI::ImageViewCreateInfo imageViewCI;
            imageViewCI.image = m_graphicsAPI->GetSwapchainImage(m_depthSwapchainInfo.swapchain, j);
            imageViewCI.type = GraphicsAPI::ImageViewCreateInfo::Type::DSV;
            imageViewCI.view = GraphicsAPI::ImageViewCreateInfo::View::TYPE_2D_ARRAY;
            imageViewCI.format = m_depthSwapchainInfo.swapchainFormat;
            imageViewCI.aspect = GraphicsAPI::ImageViewCreateInfo::Aspect::DEPTH_BIT;
            imageViewCI.baseMipLevel = 0;
            imageViewCI.levelCount = 1;
            imageViewCI.baseArrayLayer = 0;
            imageViewCI.layerCount = viewCount;
            m_depthSwapchainInfo.imageViews.push_back(m_graphicsAPI->CreateImageView(imageViewCI));
        }

        XR_LOG("Created swapchains with reccomended resolution: " << viewConfigurationView.recommendedImageRectWidth << "x" << viewConfigurationView.recommendedImageRectHeight);
    }

    void DestroySwapchains() {
        // Destroy the color and depth image views from GraphicsAPI.
        for (void*& imageView : m_colorSwapchainInfo.imageViews) {
            m_graphicsAPI->DestroyImageView(imageView);
        }
        for (void*& imageView : m_depthSwapchainInfo.imageViews) {
            m_graphicsAPI->DestroyImageView(imageView);
        }

        // Free the Swapchain Image Data.
        m_graphicsAPI->FreeSwapchainImageData(m_colorSwapchainInfo.swapchain);
        m_graphicsAPI->FreeSwapchainImageData(m_depthSwapchainInfo.swapchain);

        // Destroy the swapchains.
        OPENXR_CHECK(xrDestroySwapchain(m_colorSwapchainInfo.swapchain), "Failed to destroy Color Swapchain");
        OPENXR_CHECK(xrDestroySwapchain(m_depthSwapchainInfo.swapchain), "Failed to destroy Depth Swapchain");
    }

    virtual void HandleInteractions() {}

    virtual void OnRender(double now, double dt) {}

    void RenderFrame() {
        // Get the XrFrameState for timing and rendering info.
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        OPENXR_CHECK(xrWaitFrame(m_session, &frameWaitInfo, &frameState), "Failed to wait for XR Frame.");

        // Tell the OpenXR compositor that the application is beginning the frame.
        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        OPENXR_CHECK(xrBeginFrame(m_session, &frameBeginInfo), "Failed to begin the XR Frame.");

        // Variables for rendering and layer composition.
        bool rendered = false;
        RenderLayerInfo renderLayerInfo;
        renderLayerInfo.predictedDisplayTime = frameState.predictedDisplayTime;

        // Check that the session is active and that we should render.
        bool sessionActive = (m_sessionState == XR_SESSION_STATE_SYNCHRONIZED || m_sessionState == XR_SESSION_STATE_VISIBLE || m_sessionState == XR_SESSION_STATE_FOCUSED);
        if (sessionActive && frameState.shouldRender) {
            // poll actions here because they require a predicted display time, which we've only just obtained.
            PollActionsInternal(frameState.predictedDisplayTime);
            // Handle interactions.
            HandleInteractions();
            // Render the stereo image and associate one of swapchain images with the XrCompositionLayerProjection structure.
            rendered = RenderLayer(renderLayerInfo);
            if (rendered) {
                renderLayerInfo.layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&renderLayerInfo.layerProjection));
            }
        }

        // Tell OpenXR that we are finished with this frame; specifying its display time, environment blending and layers.
        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.displayTime = frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = m_environmentBlendMode;
        frameEndInfo.layerCount = static_cast<uint32_t>(renderLayerInfo.layers.size());
        frameEndInfo.layers = renderLayerInfo.layers.data();
        OPENXR_CHECK(xrEndFrame(m_session, &frameEndInfo), "Failed to end the XR Frame.");
    }

    bool RenderLayer(RenderLayerInfo &renderLayerInfo) {
        // Locate the views from the view configuration within the (reference) space at the display time.
        std::vector<XrView> views(m_viewConfigurationViews.size(), {XR_TYPE_VIEW});

        XrViewState viewState{XR_TYPE_VIEW_STATE};  // Will contain information on whether the position and/or orientation is valid and/or tracked.
        XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        viewLocateInfo.viewConfigurationType = m_viewConfiguration;
        viewLocateInfo.displayTime = renderLayerInfo.predictedDisplayTime;
        viewLocateInfo.space = m_localSpace;
        uint32_t viewCount = 0;
        XrResult result = xrLocateViews(m_session, &viewLocateInfo, &viewState, static_cast<uint32_t>(views.size()), &viewCount, views.data());
        if (result != XR_SUCCESS) {
            XR_LOG("Failed to locate Views.");
            return false;
        }

        // Resize the layer projection views to match the view count. The layer projection views are used in the layer projection.
        renderLayerInfo.layerProjectionViews.resize(viewCount, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

        for (uint32_t i = 0; i < viewCount; i++) {
            // Apply the camera position offset to the view.
            views[i].pose.position.x += cameraPositionOffset.x;
            views[i].pose.position.y += cameraPositionOffset.y;
            views[i].pose.position.z += cameraPositionOffset.z;
        }
        for (int i = 0; i < 2; i++) {
            m_handNodes[i].setPosition(m_handNodes[i].getPosition() + cameraPositionOffset);
        }

        // Acquire and wait for an image from the swapchains.
        // Get the image index of an image in the swapchains.
        // The timeout is infinite.
        uint32_t colorImageIndex = 0;
        uint32_t depthImageIndex = 0;
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        OPENXR_CHECK(xrAcquireSwapchainImage(m_colorSwapchainInfo.swapchain, &acquireInfo, &colorImageIndex), "Failed to acquire Image from the Color Swapchian");
        OPENXR_CHECK(xrAcquireSwapchainImage(m_depthSwapchainInfo.swapchain, &acquireInfo, &depthImageIndex), "Failed to acquire Image from the Depth Swapchian");

        XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        OPENXR_CHECK(xrWaitSwapchainImage(m_colorSwapchainInfo.swapchain, &waitInfo), "Failed to wait for Image from the Color Swapchain");
        OPENXR_CHECK(xrWaitSwapchainImage(m_depthSwapchainInfo.swapchain, &waitInfo), "Failed to wait for Image from the Depth Swapchain");

        // Get the width and height and construct the viewport and scissors.
        const uint32_t &width = m_viewConfigurationViews[0].recommendedImageRectWidth;
        const uint32_t &height = m_viewConfigurationViews[0].recommendedImageRectHeight;

        // Fill out the XrCompositionLayerProjectionView structure specifying the pose and fov from the view.
        // This also associates the swapchain image with this layer projection view.
        // Per view in the view configuration:
        for (uint32_t i = 0; i < viewCount; i++) {
            renderLayerInfo.layerProjectionViews[i] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
            renderLayerInfo.layerProjectionViews[i].pose = views[i].pose;
            renderLayerInfo.layerProjectionViews[i].fov = views[i].fov;
            renderLayerInfo.layerProjectionViews[i].subImage.swapchain = m_colorSwapchainInfo.swapchain;
            renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.x = 0;
            renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.y = 0;
            renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.width = static_cast<int32_t>(width);
            renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.height = static_cast<int32_t>(height);
            renderLayerInfo.layerProjectionViews[i].subImage.imageArrayIndex = i;  // Useful for multiview rendering.
        }

        // Prepare to render
        glViewport(0, 0, width, height);
        m_graphicsAPI->resize(width, height);
        m_graphicsAPI->SetRenderAttachments(&m_colorSwapchainInfo.imageViews[colorImageIndex], 1, m_depthSwapchainInfo.imageViews[depthImageIndex], width, height);

        // Update vr cameras
        cameras->setProjectionMatrices({
            gxi::toGLM(views[0].fov, m_apiType, nearZ, farZ),
            gxi::toGLM(views[1].fov, m_apiType, nearZ, farZ)
        });
        cameras->setViewMatrices({
            glm::inverse(gxi::toGlm(views[0].pose)),
            glm::inverse(gxi::toGlm(views[1].pose))
        });

        double now = renderLayerInfo.predictedDisplayTime / 1e+9; // Convert nanoseconds to seconds.
        static double lastTime = now;
        double dt = (now - lastTime);
        OnRender(now, dt);
        lastTime = now;

        // Give the swapchain image back to OpenXR, allowing the compositor to use the image.
        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        OPENXR_CHECK(xrReleaseSwapchainImage(m_colorSwapchainInfo.swapchain, &releaseInfo), "Failed to release Image back to the Color Swapchain");
        OPENXR_CHECK(xrReleaseSwapchainImage(m_depthSwapchainInfo.swapchain, &releaseInfo), "Failed to release Image back to the Depth Swapchain");

        // Fill out the XrCompositionLayerProjection structure for usage with xrEndFrame().
        renderLayerInfo.layerProjection.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
        renderLayerInfo.layerProjection.space = m_localSpace;
        renderLayerInfo.layerProjection.viewCount = static_cast<uint32_t>(renderLayerInfo.layerProjectionViews.size());
        renderLayerInfo.layerProjection.views = renderLayerInfo.layerProjectionViews.data();

        return true;
    }

public:
    // Stored pointer to the android_app structure from android_main().
    static android_app* androidApp;

    // Custom data structure that is used by PollSystemEvents().
    // Modified from https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/d6b6d7a10bdcf8d4fe806b4f415fde3dd5726878/src/tests/hello_xr/main.cpp#L133C1-L189C2
    struct AndroidAppState {
        ANativeWindow* nativeWindow = nullptr;
        bool resumed = false;
    };
    static AndroidAppState androidAppState;

    // Processes the next command from the Android OS. It updates AndroidAppState.
    static void AndroidAppHandleCmd(struct android_app* app, int32_t cmd) {
        AndroidAppState* appState = (AndroidAppState*)app->userData;

        switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the application thread from onCreate().
        // The application thread then calls android_main().
        case APP_CMD_START: {
            break;
        }
        case APP_CMD_RESUME: {
            appState->resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            appState->resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            break;
        }
        case APP_CMD_DESTROY: {
            appState->nativeWindow = nullptr;
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            appState->nativeWindow = app->window;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            appState->nativeWindow = nullptr;
            break;
        }
        }
    }

protected:
    void PollSystemEvents() {
        // Checks whether Android has requested that application should by destroyed.
        if (androidApp->destroyRequested != 0) {
            m_applicationRunning = false;
            return;
        }
        while (true) {
            // Poll and process the Android OS system events.
            struct android_poll_source* source = nullptr;
            int events = 0;
            // The timeout depends on whether the application is active.
            const int timeoutMilliseconds = (!androidAppState.resumed && !m_sessionRunning && androidApp->destroyRequested == 0) ? -1 : 0;
            if (ALooper_pollAll(timeoutMilliseconds, nullptr, &events, (void* *)&source) >= 0) {
                if (source != nullptr) {
                    source->process(androidApp, source);
                }
            }
            else {
                break;
            }
        }
    }

    XrInstance m_xrInstance = XR_NULL_HANDLE;
    std::vector<const char*> m_activeAPILayers = {};
    std::vector<const char*> m_activeInstanceExtensions = {};
    std::vector<std::string> m_apiLayers = {};
    std::vector<std::string> m_instanceExtensions = {};

    XrDebugUtilsMessengerEXT m_debugUtilsMessenger = XR_NULL_HANDLE;

    XrFormFactor m_formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId m_systemID = {};
    XrSystemProperties m_systemProperties = {XR_TYPE_SYSTEM_PROPERTIES};

    GraphicsAPI_Type m_apiType = UNKNOWN;
    std::unique_ptr<GraphicsAPI> m_graphicsAPI = nullptr;

    XrSession m_session = {};
    XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;
    bool m_applicationRunning = true;
    bool m_sessionRunning = false;

    std::vector<XrViewConfigurationType> m_applicationViewConfigurations = {XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO};
    std::vector<XrViewConfigurationType> m_viewConfigurations;
    XrViewConfigurationType m_viewConfiguration = XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM;
    std::vector<XrViewConfigurationView> m_viewConfigurationViews;

    struct SwapchainInfo {
        XrSwapchain swapchain = XR_NULL_HANDLE;
        int64_t swapchainFormat = 0;
        std::vector<void*> imageViews;
    };
    SwapchainInfo m_colorSwapchainInfo = {};
    SwapchainInfo m_depthSwapchainInfo = {};

    std::vector<XrEnvironmentBlendMode> m_applicationEnvironmentBlendModes = {XR_ENVIRONMENT_BLEND_MODE_OPAQUE, XR_ENVIRONMENT_BLEND_MODE_ADDITIVE};
    std::vector<XrEnvironmentBlendMode> m_environmentBlendModes = {};
    XrEnvironmentBlendMode m_environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;

    XrSpace m_localSpace = XR_NULL_HANDLE;
    struct RenderLayerInfo {
        XrTime predictedDisplayTime = 0;
        std::vector<XrCompositionLayerBaseHeader*> layers;
        XrCompositionLayerProjection layerProjection = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        std::vector<XrCompositionLayerProjectionView> layerProjectionViews;
    };

    float nearZ = 0.05f;
    float farZ = 1000.0f;

    Config config;

    std::unique_ptr<VRCamera> cameras;
    std::unique_ptr<Scene> scene;

    glm::vec3 cameraPositionOffset{0.0f, 0.0f, 0.0f};

    // In STAGE space, viewHeightM should be 0. In LOCAL space, it should be offset downwards, below the viewer's initial position.
    float m_viewHeightM = 1.6f;

    XrActionSet m_actionSet;
    // The action for getting the hand or controller position and orientation.
    XrAction m_palmPoseAction;
    // The XrPaths for left and right hand hands or controllers.
    XrPath m_handPaths[2] = {0, 0};
    // The spaces that represents the two hand poses.
    XrSpace m_handPoseSpace[2];
    XrActionStatePose m_handPoseState[2] = {{XR_TYPE_ACTION_STATE_POSE}, {XR_TYPE_ACTION_STATE_POSE}};
    // The current poses obtained from the XrSpaces.
    Node m_handNodes[2];
};

#endif // OPENXR_APP_H
