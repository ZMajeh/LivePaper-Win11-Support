#pragma once

#include <stdbool.h>

// The functions that open the files and the folders with the program that
// belongs to them. They are the shell integration of the dock bar.

// Opens the file with the program that belongs to it (an image with the
// image viewer, a video with the player, a text with the editor).
bool LauncherOpenFile(const char *path);

// Opens the folder in the file manager of the system.
bool LauncherOpenFolder(const char *path);

// Opens the file manager of the system, in the given folder (or in the home
// folder of the user, if the path is NULL).
bool LauncherOpenFileManager(const char *path);

// Opens the native file dialog of the system and lets the user choose a file
// (only files, not folders). The dialog runs on its own thread, so that the
// program keeps running while it is open. The chosen path is delivered
// asynchronously to the main thread with an SDL event; the main loop has to
// handle the event and add the chosen path to the bar. The function returns
// NULL immediately (the result is not available synchronously). On platforms
// without a native dialog it returns NULL.
// The caller does not have to free anything.
char *LauncherPickFile(void);

// Opens the native folder dialog of the system and lets the user choose a
// folder (only folders, not files). Same delivery mechanism as
// LauncherPickFile().
char *LauncherPickFolder(void);

// The home folder of the user. The caller has to free the string.
char *LauncherHomeFolder(void);
