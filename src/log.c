#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static enum LogLevel g_level = LOG_LEVEL_INFO;
static FILE *g_file = NULL;

static const char *LevelName(enum LogLevel level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN ";
        case LOG_LEVEL_INFO:  return "INFO ";
        case LOG_LEVEL_DEBUG: return "DEBUG";
    }
    return "?????";
}

void LogInit(enum LogLevel level, const char *file_path) {
    g_level = level;
    if (file_path && file_path[0]) {
        g_file = fopen(file_path, "a");
        if (!g_file)
            fprintf(stderr, "live-paper: can't open log file '%s'\n", file_path);
    }
}

void LogShutdown(void) {
    if (g_file) {
        fclose(g_file);
        g_file = NULL;
    }
}

void LogAttachParentConsole(void) {
#ifdef _WIN32
    // A GUI application doesn't get a console of its own, and the standard
    // streams of the process are not usable. Attach to the console of the
    // parent process, so that the messages end up in the terminal that the
    // program was started from.
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;   // no parent console, nothing to do

    // The standard streams have to be reopened, because they don't refer to
    // the console that we have just attached to.
    if (!freopen("CONOUT$", "w", stdout))
        return;
    if (!freopen("CONOUT$", "w", stderr))
        freopen("CONERR$", "w", stderr);
#endif
}

void LogSetLevel(enum LogLevel level) {
    g_level = level;
}

enum LogLevel LogGetLevel(void) {
    return g_level;
}

void LogMessage(enum LogLevel level, const char *fmt, ...) {
    if (level > g_level) return;

    char body[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);

    char stamp[32] = "";
    time_t now = time(NULL);
    struct tm tm_buf;
#ifdef _WIN32
    if (localtime_s(&tm_buf, &now) == 0)
#else
    if (localtime_r(&now, &tm_buf))
#endif
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    char line[2304];
    snprintf(line, sizeof(line), "[%s] %s: %s", stamp, LevelName(level), body);

    fprintf(stderr, "%s\n", line);
    fflush(stderr);
    if (g_file) {
        fprintf(g_file, "%s\n", line);
        fflush(g_file);
    }
#ifdef _WIN32
    OutputDebugStringA(line);
    OutputDebugStringA("\n");
#endif
}