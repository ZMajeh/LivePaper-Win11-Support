#pragma once

#include <stdbool.h>

// The listing of a folder. It is used by the popup of the dock bar (the list
// of the files of an added folder) and by the file picker (the list of the
// files of the folder that is shown).

struct DirEntry {
    char *name;      // the name of the file or of the folder
    bool is_dir;     // true for a folder
};

// Reads the content of the given folder. The entries are sorted: the folders
// first, then the files, both in the alphabetical order.
// The result has to be freed with DirListFree(). Returns the number of the
// entries, or -1 if the folder could not be read.
int DirListRead(const char *path, struct DirEntry **out);

// Frees a list of the entries.
void DirListFree(struct DirEntry *entries, int count);

// Whether the given path is a folder.
bool DirIsFolder(const char *path);

// Joins the folder and the name of a file. The caller has to free the
// result.
char *DirJoinPath(const char *folder, const char *name);

// The name of the file or of the folder, without the path.
const char *DirBaseName(const char *path);

// The folder of the given path, without the file name. The caller has to
// free the result. For a path in the root of a drive it returns the root.
char *DirParentPath(const char *path);
