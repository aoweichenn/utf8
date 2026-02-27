//
// Created by aoweichen on 2026/2/27.
//

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#ifdef UTF8_BUILD_SHARED
#define UTF8_API __declspec(dllexport)
#elif defined(UTF8_USE_SHARED)
#define UTF8_API __declspec(dllimport)
#else
#define UTF8_API
#endif
#else
#ifdef UTF8_BUILD_SHARED
#define UTF8_API __attribute__((visibility("default")))
#else
#define UTF8_API
#endif
#endif
