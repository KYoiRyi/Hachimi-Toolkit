#pragma once

#ifndef _WIN32

#include <stdint.h>
#include <string>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

#define __try try
#define EXCEPTION_EXECUTE_HANDLER 1
#define __except(x) catch(...)

typedef uint32_t DWORD;
typedef uint64_t DWORDLONG;
typedef void* HANDLE;

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

inline uint32_t GetTickCount() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

struct SYSTEM_INFO {
    DWORD dwNumberOfProcessors;
};

inline void GetSystemInfo(SYSTEM_INFO* si) {
    si->dwNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);
}

struct MEMORYSTATUSEX {
    DWORD dwLength;
    DWORDLONG ullTotalPhys;
    DWORDLONG ullAvailPhys;
};

inline bool GlobalMemoryStatusEx(MEMORYSTATUSEX* ms) {
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    ms->ullTotalPhys = (DWORDLONG)pages * page_size;
    
    long avail_pages = sysconf(_SC_AVPHYS_PAGES);
    ms->ullAvailPhys = (DWORDLONG)avail_pages * page_size;
    return true;
}

struct WIN32_FIND_DATAA {
    char cFileName[256];
};

inline HANDLE FindFirstFileA(const char* lpFileName, WIN32_FIND_DATAA* lpFindFileData) {
    // We only need this to not crash, actually doing FindFirstFile on Android can be stubbed
    // or we can implement a basic one using opendir.
    // For now, let's just return INVALID_HANDLE_VALUE to disable whatever cleanup it does.
    return INVALID_HANDLE_VALUE;
}

inline bool FindNextFileA(HANDLE hFindFile, WIN32_FIND_DATAA* lpFindFileData) {
    return false;
}

inline void FindClose(HANDLE hFindFile) {}
inline bool DeleteFileA(const char* lpFileName) { return unlink(lpFileName) == 0; }

inline int fopen_s(FILE** pFile, const char *filename, const char *mode) {
    *pFile = fopen(filename, mode);
    return *pFile ? 0 : errno;
}

#define CP_UTF8 65001

template<typename T>
inline int WideCharToMultiByte(
    unsigned int CodePage,
    DWORD    dwFlags,
    const T* lpWideCharStr,
    int      cchWideChar,
    char*    lpMultiByteStr,
    int      cbMultiByte,
    const char* lpDefaultChar,
    bool* lpUsedDefaultChar
) {
    if (cbMultiByte == 0) return cchWideChar * 3;
    int outIdx = 0;
    for (int i = 0; i < cchWideChar; ++i) {
        uint16_t c = (uint16_t)lpWideCharStr[i];
        if (c < 0x80) {
            if (outIdx < cbMultiByte) lpMultiByteStr[outIdx++] = (char)c;
        } else if (c < 0x800) {
            if (outIdx < cbMultiByte) lpMultiByteStr[outIdx++] = (char)(0xC0 | (c >> 6));
            if (outIdx < cbMultiByte) lpMultiByteStr[outIdx++] = (char)(0x80 | (c & 0x3F));
        } else {
            if (outIdx < cbMultiByte) lpMultiByteStr[outIdx++] = (char)(0xE0 | (c >> 12));
            if (outIdx < cbMultiByte) lpMultiByteStr[outIdx++] = (char)(0x80 | ((c >> 6) & 0x3F));
            if (outIdx < cbMultiByte) lpMultiByteStr[outIdx++] = (char)(0x80 | (c & 0x3F));
        }
    }
    return outIdx;
}

#define Sleep(ms) usleep((ms) * 1000)

template<typename... Args>
inline int sprintf_s(char* buffer, const char* format, Args... args) {
    return sprintf(buffer, format, args...);
}

struct SYSTEMTIME {
    int wYear;
    int wMonth;
    int wDay;
    int wHour;
    int wMinute;
    int wSecond;
};

inline void GetLocalTime(SYSTEMTIME* st) {
    time_t t = time(NULL);
    struct tm tm_st;
    localtime_r(&t, &tm_st);
    st->wYear = tm_st.tm_year + 1900;
    st->wMonth = tm_st.tm_mon + 1;
    st->wDay = tm_st.tm_mday;
    st->wHour = tm_st.tm_hour;
    st->wMinute = tm_st.tm_min;
    st->wSecond = tm_st.tm_sec;
}

#endif
