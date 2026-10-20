#pragma once

// Diagnostics logging.
//
// Messages are printed to stderr and, if a log file was configured with
// LogInit(), additionally appended to that file. On Windows the messages are
// also sent to the debugger via OutputDebugStringA().

enum LogLevel {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
};

// Opens the log file (if path is not NULL/empty) and sets the minimum level
// of messages that are reported.
void LogInit(enum LogLevel level, const char *file_path);
void LogShutdown(void);

// On Windows the program is built as a GUI application, which means that its
// standard output is not connected to anything when it is started. This
// function attaches the process to the console of the parent process, so that
// the diagnostics are visible in the terminal the program was started from.
// It does nothing if there is no parent console (e.g. when the program is
// started from the file manager) or on other platforms.
void LogAttachParentConsole(void);

void LogSetLevel(enum LogLevel level);
enum LogLevel LogGetLevel(void);

void LogMessage(enum LogLevel level, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 2, 3)))
#endif
    ;

#define LOG_ERROR(...) LogMessage(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARN(...) LogMessage(LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_INFO(...) LogMessage(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DEBUG(...) LogMessage(LOG_LEVEL_DEBUG, __VA_ARGS__)