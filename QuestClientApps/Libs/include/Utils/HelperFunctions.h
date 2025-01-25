// Copyright 2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

// OpenXR Tutorial for Khronos Group

#pragma once

// C/C++ Headers
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

// Debugbreak
#include <signal.h>
#define DEBUG_BREAK raise(SIGTRAP)

inline bool IsStringInVector(std::vector<const char *> list, const char *name) {
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

template <typename T>
T Align(T value, T alignment) {
    return (value + (alignment - 1)) & ~(alignment - 1);
};
