// Copyright 2023, The Khronos Group Inc
//
// SPDX-License-Identifier: Apache-2.0

// OpenXR Tutorial for Khronos Group

#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include <android/log.h>

class AndroidStreambuf : public std::streambuf {
public:
    explicit AndroidStreambuf(android_LogPriority priority = ANDROID_LOG_DEBUG)
        : logPriority(priority) {}

private:
    int_type overflow(int_type character) override {
        if (!traits_type::eq_int_type(character, traits_type::eof())) {
            const char value = traits_type::to_char_type(character);
            if (value == '\n') {
                if (sync() != 0) {
                    return traits_type::eof();
                }
            } else {
                pending += value;
            }
        }
        return traits_type::not_eof(character);
    }
    int sync() override {
        if (!pending.empty()) {
            if (__android_log_write(logPriority, "quest_client", pending.c_str()) < 0) {
                return -1;
            }
            pending.clear();
        }
        return 0;
    }

    android_LogPriority logPriority;
    std::string pending;
};

class DebugOutput {
public:
    DebugOutput()
        : androidCerr(ANDROID_LOG_ERROR)
        , previousCout(std::cout.rdbuf(&androidCout))
        , previousCerr(std::cerr.rdbuf(&androidCerr)) {}

    ~DebugOutput() {
        androidCout.pubsync();
        androidCerr.pubsync();
        std::cout.rdbuf(previousCout);
        std::cerr.rdbuf(previousCerr);
    }

    DebugOutput(const DebugOutput&) = delete;
    DebugOutput& operator=(const DebugOutput&) = delete;

private:
    AndroidStreambuf androidCout;
    AndroidStreambuf androidCerr;
    std::streambuf* previousCout;
    std::streambuf* previousCerr;
};

#define XR_LOG_TAG "oculus_client"
#define XR_LOG(...) {                                                           \
        std::ostringstream ostr;                                                    \
        ostr<<__VA_ARGS__;                                                          \
        __android_log_write(ANDROID_LOG_DEBUG, XR_LOG_TAG, ostr.str().c_str()); \
    }
#define XR_LOG_ERROR(...) {                                                     \
        std::ostringstream ostr;                                                    \
        ostr<<__VA_ARGS__;                                                          \
        __android_log_write(ANDROID_LOG_ERROR, XR_LOG_TAG, ostr.str().c_str()); \
    }
