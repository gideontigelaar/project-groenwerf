#pragma once
#include <cstdio>

#ifndef ENABLE_LOGGING
#define ENABLE_LOGGING 1
#endif

#if ENABLE_LOGGING
    #define LOG_INFO(fmt, ...)  printf("[INFO] " fmt "\n", ##__VA_ARGS__)
    #define LOG_WARN(fmt, ...)  printf("[WARN] " fmt "\n", ##__VA_ARGS__)
    #define LOG_ERROR(fmt, ...) printf("[ERR ] " fmt "\n", ##__VA_ARGS__)
    #define LOG_RAW(fmt, ...)   printf(fmt, ##__VA_ARGS__) // raw output, no prefix
#else
    #define LOG_INFO(fmt, ...)
    #define LOG_WARN(fmt, ...)
    #define LOG_ERROR(fmt, ...)
    #define LOG_RAW(fmt, ...)
#endif