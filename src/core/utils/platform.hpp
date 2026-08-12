#pragma once

#ifdef _WIN32
    #include <io.h>
    #include <windows.h>
    
    inline int platform_fsync(int fd) {
        HANDLE h = (HANDLE)_get_osfhandle(fd);
        if (h == INVALID_HANDLE_VALUE) return -1;
        if (!FlushFileBuffers(h)) return -1;
        return 0;
    }
#else
    #include <unistd.h>
    inline int platform_fsync(int fd) {
        return fsync(fd);
    }
#endif