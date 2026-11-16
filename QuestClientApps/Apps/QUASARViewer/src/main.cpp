#include <QUASARViewer.h>

android_app* OpenXRApp::androidApp = nullptr;
OpenXRApp::AndroidAppState OpenXRApp::androidAppState = {};

void OpenXRApp_Main() {
    DebugOutput debugOutput; // Redirect std::cerr and std::cout to logcat.
    XR_LOG("Starting QUASARViewer...");

    QUASARViewer app;
    app.Run();
}

void android_main(struct android_app* app) {
    // Allow interaction with JNI and the JVM on this thread
    JNIEnv* env;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    // Look up the Android loader initialization function.
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

    // Register the Android asset manager for FileIO.
    FileIO::registerIOSystem(app->activity);

    OpenXRApp::androidApp = app;
    OpenXRApp_Main();
}
