// core/utils/platform.hpp — добавить рядом с platform_fsync
#pragma once

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <fcntl.h>

inline int platform_fsync(int fd) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    if (!FlushFileBuffers(h)) return -1;
    return 0;
}

constexpr int PLATFORM_O_BINARY = O_BINARY;
#else
#include <unistd.h>

inline int platform_fsync(int fd) {
    return fsync(fd);
}

constexpr int PLATFORM_O_BINARY = 0; // на POSIX бинарный режим не отличается от обычного
#endif