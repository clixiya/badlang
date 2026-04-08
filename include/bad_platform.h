#ifndef BAD_PLATFORM_H
#define BAD_PLATFORM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline char *bad_strdup_local(const char *s) {
    size_t len = s ? strlen(s) : 0;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    if (len) memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>

#define BAD_STRCASECMP _stricmp
#define BAD_STRDUP bad_strdup_local

static inline int bad_isatty_stdout(void) {
    if (!_isatty(_fileno(stdout))) return 0;

    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    return 1;
}

static inline void bad_sleep_ms(int ms) {
    if (ms > 0) Sleep((DWORD)ms);
}

static inline long bad_monotonic_ms(void) {
    return (long)GetTickCount64();
}

static inline long bad_epoch_ms(void) {
    const unsigned long long windows_epoch_100ns = 116444736000000000ULL;
    FILETIME ft;
    ULARGE_INTEGER ticks;

    GetSystemTimeAsFileTime(&ft);
    ticks.LowPart = ft.dwLowDateTime;
    ticks.HighPart = ft.dwHighDateTime;

    if (ticks.QuadPart < windows_epoch_100ns) return 0;
    return (long)((ticks.QuadPart - windows_epoch_100ns) / 10000ULL);
}

static inline int bad_mkdir(const char *path) {
    return _mkdir(path);
}

#else
#include <strings.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define BAD_STRCASECMP strcasecmp
#define BAD_STRDUP bad_strdup_local

static inline int bad_isatty_stdout(void) {
    return isatty(STDOUT_FILENO) ? 1 : 0;
}

static inline void bad_sleep_ms(int ms) {
    if (ms <= 0) return;
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    (void)select(0, NULL, NULL, NULL, &tv);
}

static inline long bad_monotonic_ms(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) return 0;
    return (long)(tv.tv_sec * 1000L + tv.tv_usec / 1000L);
}

static inline long bad_epoch_ms(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) return 0;
    return (long)(tv.tv_sec * 1000L + tv.tv_usec / 1000L);
}

static inline int bad_mkdir(const char *path) {
    return mkdir(path, 0755);
}
#endif


#endif