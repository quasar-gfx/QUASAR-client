#pragma once

enum android_LogPriority {
    ANDROID_LOG_DEBUG = 3,
    ANDROID_LOG_WARN = 5,
    ANDROID_LOG_ERROR = 6,
};

extern "C" int __android_log_write(int priority, const char* tag, const char* message);
