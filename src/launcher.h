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

// The home folder of the user. The caller has to free the string.
char *LauncherHomeFolder(void);
