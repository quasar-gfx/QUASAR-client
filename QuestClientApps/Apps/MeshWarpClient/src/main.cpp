#include <MeshWarpClient.h>

android_app* OpenXRApp::androidApp = nullptr;
OpenXRApp::AndroidAppState OpenXRApp::androidAppState = {};

void OpenXRApp_Main() {
    DebugOutput debugOutput; // This redirects std::cerr and std::cout to the IDE's output or Android Studio's logcat.
    XR_LOG("Starting MeshWarpClient...");

    MeshWarpClient app;
    app.Run();
}

void android_main(struct android_app* app) {
    // Allow interaction with JNI and the JVM on this thread
    // Https://developer.androidcom/training/articles/perf-jni#threads
    JNIEnv* env;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    // Https://registry.khronos.org/OpenXR/specs/1.0/html/xrspechtml#XR_KHR_loader_init
    // Load xrInitializeLoaderKHR() function pointer. On Android, the loader must be initialized with variables from android_app*
    // Without this, there's is no loader and thus our function calls to OpenXR would fail
    XrInstance xrInstance = XR_NULL_HANDLE;  // Dummy XrInstance variable for OPENXR_CHECK macro
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR = nullptr;
    OPENXR_CHECK(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&xrInitializeLoaderKHR), "Failed to get InstanceProcAddr for xrInitializeLoaderKHR.");
    if (!xrInitializeLoaderKHR) {
        return;
    }

    // Fill out an XrLoaderInitInfoAndroidKHR structure and initialize the loader for Android
    XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
    loaderInitializeInfoAndroid.applicationVM = app->activity->vm;
    loaderInitializeInfoAndroid.applicationContext = app->activity->clazz;
    OPENXR_CHECK(xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfoAndroid), "Failed to initialize Loader for Android.");

    // Set userData and Callback for PollSystemEvents()
    app->userData = &OpenXRApp::androidAppState;
    app->onAppCmd = OpenXRApp::AndroidAppHandleCmd;

    // Set the asset manager for FileIO (required in order to load files from the Android filesystem)
    FileIO::registerIOSystem(app->activity);

    // Initialize GStreamer Android
    VideoTexture::gst_android_glue_init(app->activity);

    OpenXRApp::androidApp = app;
    OpenXRApp_Main();
}
