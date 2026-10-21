#include "launcher.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#elif defined(__APPLE__)
#include <unistd.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#ifdef _WIN32

bool LauncherOpenFile(const char *path) {
    if (!path || !*path) return false;
    // "open" is the default verb: the shell decides which program is used.
    HINSTANCE result = ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
    // The value is greater than 32 if the program was started.
    if ((INT_PTR)result <= 32) {
        LOG_WARN("can't open '%s' (the error of the shell is %d)", path, (int)(INT_PTR)result);
        return false;
    }
    LOG_INFO("the file '%s' was opened", path);
    return true;
}

bool LauncherOpenFolder(const char *path) {
    if (!path || !*path) return false;
    HINSTANCE result = ShellExecuteA(NULL, "explore", path, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        // Not every system has the "explore" verb, so the plain "open" is
        // tried as well.
        result = ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
    }
    if ((INT_PTR)result <= 32) {
        LOG_WARN("can't open the folder '%s'", path);
        return false;
    }
    LOG_INFO("the folder '%s' was opened", path);
    return true;
}

bool LauncherOpenFileManager(const char *path) {
    if (path && *path) return LauncherOpenFolder(path);
    char *home = LauncherHomeFolder();
    if (!home) return false;
    bool result = LauncherOpenFolder(home);
    free(home);
    return result;
}

char *LauncherHomeFolder(void) {
    char path[MAX_PATH] = {0};
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) return NULL;
    return strdup(path);
}

#else  // !_WIN32

// Starts the program with the given arguments, without waiting for it.
static void StartProcess(const char *program, const char *argument) {
    pid_t pid = fork();
    if (pid < 0) {
        LOG_WARN("can't start '%s': %s", program, strerror(errno));
        return;
    }
    if (pid == 0) {
        // The child process: it must not inherit the standard streams, or it
        // would write into the log of the program.
        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        if (argument) {
            execlp(program, program, argument, (char *)NULL);
        } else {
            execlp(program, program, (char *)NULL);
        }
        _exit(127);
    }
    // The parent process continues: the program runs on its own.
}

bool LauncherOpenFile(const char *path) {
    if (!path || !*path) return false;
    StartProcess("xdg-open", path);
    LOG_INFO("the file '%s' was opened", path);
    return true;
}

bool LauncherOpenFolder(const char *path) {
    if (!path || !*path) return false;
    StartProcess("xdg-open", path);
    LOG_INFO("the folder '%s' was opened", path);
    return true;
}

bool LauncherOpenFileManager(const char *path) {
    if (path && *path) return LauncherOpenFolder(path);
    char *home = LauncherHomeFolder();
    if (!home) return false;
    bool result = LauncherOpenFolder(home);
    free(home);
    return result;
}

char *LauncherHomeFolder(void) {
    const char *home = getenv("HOME");
    if (!home || !*home) return NULL;
    return strdup(home);
}

#endif  // !_WIN32
