// Copyright 2023, The Khronos Group Inc
//
// SPDX-License-Identifier: Apache-2.0

// OpenXR Tutorial for Khronos Group

#pragma once

// C/C++ Headers
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

// Debugbreak
#include <signal.h>
#define DEBUG_BREAK raise(SIGTRAP)

inline bool IsStringInVector(const std::vector<const char*>& list, const char* name) {
    bool found = false;
    for (auto &item : list) {
        if (strcmp(name, item) == 0) {
            found = true;
            break;
        }
    }
    return found;
}

template <typename T>
inline bool BitwiseCheck(const T &value, const T &checkValue) {
    return ((value & checkValue) == checkValue);
}
