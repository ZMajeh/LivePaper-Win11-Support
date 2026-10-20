#include "fail.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

void MessageAndQuit(int line, const char *file, const char *message, ...) {
    va_list args;
    va_start(args, message);

    // The safe versions of the functions are used: the message comes from the
    // program itself, but a long path or a long message would overflow the
    // buffer with sprintf().
    char buf[4096] = {0};
    int pos = snprintf(buf, sizeof(buf), "Error %s:%i", file, line);
    if (pos < 0) pos = 0;
    if ((size_t)pos >= sizeof(buf)) pos = (int)sizeof(buf) - 1;
    if (message) {
        int written = snprintf(buf + pos, sizeof(buf) - pos, ": ");
        if (written > 0) pos += written;
        if ((size_t)pos >= sizeof(buf)) pos = (int)sizeof(buf) - 1;
        written = vsnprintf(buf + pos, sizeof(buf) - pos, message, args);
        if (written > 0) pos += written;
        if ((size_t)pos >= sizeof(buf)) pos = (int)sizeof(buf) - 1;
    }
    va_end(args);

    // Also record the error in the log, so that it is visible even when there
    // is no console (e.g. on Windows) or when a log file was requested.
    LOG_ERROR("%s", buf);

#ifdef _WIN32
    MessageBoxA(NULL, buf, "LivePaper Error", MB_OK | MB_ICONERROR | MB_TASKMODAL);
#else
    puts(buf);
    puts("Exiting...");
    fflush(stdout);
#endif
    
    LogShutdown();
    exit(1);
}