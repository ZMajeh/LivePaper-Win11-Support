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
#include <shobjidl.h>
#include <SDL.h>
#elif defined(__APPLE__)
#include <unistd.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#ifdef _WIN32

// The type of the SDL event that delivers the result of the file picker.
extern Uint32 g_pick_file_event;

// The data that is passed to the thread of the file picker dialog. It
// contains the type of the SDL event that is used to deliver the result to
// the main thread.
struct PickerThreadData {
    Uint32 event_type;
};

// Runs the native file picker dialog of Windows on its own thread, so that
// the program keeps running while the dialog is open. When the user chooses
// a file or a folder, its path (UTF-8) is sent back to the main thread with
// an SDL event. When the user cancels the dialog, no event is sent.
// FOS_PICKFOLDERS is deliberately not used: it would restrict the dialog to
// folders only. Without it both files and folders can be chosen.
static int PickerThread(void *data) {
    struct PickerThreadData *thread_data = (struct PickerThreadData *)data;
    char *path = NULL;

    if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) {
        IFileOpenDialog *dialog = NULL;
        HRESULT hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                                      &IID_IFileOpenDialog, (void **)&dialog);
        if (SUCCEEDED(hr)) {
            // Both files and folders can be chosen.
            DWORD options = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
            dialog->lpVtbl->SetOptions(dialog, options);

            hr = dialog->lpVtbl->Show(dialog, NULL);
            if (SUCCEEDED(hr)) {
                IShellItem *item = NULL;
                hr = dialog->lpVtbl->GetResult(dialog, &item);
                if (SUCCEEDED(hr)) {
                    LPWSTR wide_path = NULL;
                    hr = item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH,
                                                      &wide_path);
                    if (SUCCEEDED(hr) && wide_path) {
                        int length = WideCharToMultiByte(CP_UTF8, 0, wide_path,
                                                         -1, NULL, 0, NULL,
                                                         NULL);
                        if (length > 0) {
                            path = (char *)malloc(length);
                            if (path) {
                                WideCharToMultiByte(CP_UTF8, 0, wide_path, -1,
                                                    path, length, NULL, NULL);
                            }
                        }
                        CoTaskMemFree(wide_path);
                    }
                    item->lpVtbl->Release(item);
                }
            }
            dialog->lpVtbl->Release(dialog);
        }
        CoUninitialize();
    }

    if (path && thread_data) {
        SDL_Event event;
        SDL_zero(event);
        event.type = thread_data->event_type;
        event.user.data1 = path;
        SDL_PushEvent(&event);
    } else {
        free(path);
    }
    free(thread_data);
    return 0;
}

char *LauncherPickFile(void) {
    struct PickerThreadData *thread_data = (struct PickerThreadData *)malloc(
        sizeof(*thread_data));
    if (!thread_data) return NULL;
    thread_data->event_type = g_pick_file_event;

    SDL_Thread *thread = SDL_CreateThread(PickerThread, "file-picker",
                                          thread_data);
    if (!thread) {
        free(thread_data);
        return NULL;
    }
    SDL_DetachThread(thread);
    // The result is delivered asynchronously with an SDL event.
    return NULL;
}

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

char *LauncherPickFile(void) {
    LOG_INFO("the native file picker is not available on this platform");
    return NULL;
}

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
