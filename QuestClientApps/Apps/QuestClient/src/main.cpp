// #include <SceneViewer.h>
// #include <ATWClient.h>
// #include <MeshWarpClientStatic.h>
// #include <MeshWarpClient.h>
#include <QuadsViewer.h>

android_app* OpenXRApp::androidApp = nullptr;
OpenXRApp::AndroidAppState OpenXRApp::androidAppState = {};

void OpenXRApp_Main(GraphicsAPI_Type apiType) {
    DebugOutput debugOutput; // This redirects std::cerr and std::cout to the IDE's output or Android Studio's logcat.
    XR_LOG("Starting QuestClient...");

    // SceneViewer app(apiType);
    // ATWClient app(apiType);
    // MeshWarpClientStatic app(apiType);
    // MeshWarpClient app(apiType);
    QuadsViewer app(apiType);
    app.Run();
}

void android_main(struct android_app* app) {
    // Allow interaction with JNI and the JVM on this thread.
    // https://developer.android.com/training/articles/perf-jni#threads
    JNIEnv* env;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    // https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_KHR_loader_init
    // Load xrInitializeLoaderKHR() function pointer. On Android, the loader must be initialized with variables from android_app* .
    // Without this, there's is no loader and thus our function calls to OpenXR would fail.
    XrInstance m_xrInstance = XR_NULL_HANDLE;  // Dummy XrInstance variable for OPENXR_CHECK macro.
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR = nullptr;
    OPENXR_CHECK(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&xrInitializeLoaderKHR), "Failed to get InstanceProcAddr for xrInitializeLoaderKHR.");
    if (!xrInitializeLoaderKHR) {
        return;
    }

    // Fill out an XrLoaderInitInfoAndroidKHR structure and initialize the loader for Android.
    XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
    loaderInitializeInfoAndroid.applicationVM = app->activity->vm;
    loaderInitializeInfoAndroid.applicationContext = app->activity->clazz;
    OPENXR_CHECK(xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfoAndroid), "Failed to initialize Loader for Android.");

    // Set userData and Callback for PollSystemEvents().
    app->userData = &OpenXRApp::androidAppState;
    app->onAppCmd = OpenXRApp::AndroidAppHandleCmd;

    // Set the asset manager for FileIO (required in order to load files from the Android filesystem).
    FileIO::registerIOSystem(app->activity);

    OpenXRApp::androidApp = app;
    OpenXRApp_Main(QUEST_CLIENT_GRAPHICS_API);
}
