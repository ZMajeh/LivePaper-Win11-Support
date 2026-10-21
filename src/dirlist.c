#include "dirlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// Copies the first count characters of the string. The result has to be
// freed. It is needed because strndup() is not part of C99.
static char *CopyFirst(const char *text, size_t count) {
    char *result = malloc(count + 1);
    if (!result) return NULL;
    memcpy(result, text, count);
    result[count] = '\0';
    return result;
}

// Compares two entries: the folders come first, then the files, both in the
// alphabetical order (without regard to the case).
static int CompareEntries(const void *a, const void *b) {
    const struct DirEntry *first = a;
    const struct DirEntry *second = b;
    if (first->is_dir != second->is_dir) {
        return first->is_dir ? -1 : 1;
    }
    return strcasecmp(first->name, second->name);
}

// Adds an entry to the list. The list is grown when it is full.
static bool AddEntry(struct DirEntry **entries, int *count, int *capacity,
                     const char *name, bool is_dir) {
    if (*count >= *capacity) {
        int new_capacity = *capacity ? *capacity * 2 : 32;
        struct DirEntry *grown = realloc(*entries, sizeof(struct DirEntry) * new_capacity);
        if (!grown) return false;
        *entries = grown;
        *capacity = new_capacity;
    }
    struct DirEntry *entry = &(*entries)[*count];
    entry->name = strdup(name);
    if (!entry->name) return false;
    entry->is_dir = is_dir;
    (*count)++;
    return true;
}

#ifdef _WIN32

int DirListRead(const char *path, struct DirEntry **out) {
    *out = NULL;
    if (!path || !*path) return -1;

    // The pattern of the search: every file of the folder.
    size_t length = strlen(path);
    char *pattern = malloc(length + 3);
    if (!pattern) return -1;
    // A path that ends with a backslash would get a double separator.
    if (length > 0 && path[length - 1] == '\\') {
        sprintf(pattern, "%s*", path);
    } else {
        sprintf(pattern, "%s\\*", path);
    }

    struct DirEntry *entries = NULL;
    int count = 0, capacity = 0;
    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(pattern, &data);
    free(pattern);
    if (handle == INVALID_HANDLE_VALUE) {
        LOG_WARN("the folder '%s' could not be read", path);
        return -1;
    }
    do {
        // The two special entries of the "." and the ".." are not shown.
        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) continue;
        bool is_dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        AddEntry(&entries, &count, &capacity, data.cFileName, is_dir);
    } while (FindNextFileA(handle, &data));
    FindClose(handle);

    if (count > 1) qsort(entries, count, sizeof(struct DirEntry), CompareEntries);
    *out = entries;
    return count;
}

void DirListFree(struct DirEntry *entries, int count) {
    for (int i = 0; i < count; i++) free(entries[i].name);
    free(entries);
}

bool DirIsFolder(const char *path) {
    if (!path || !*path) return false;
    DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) return false;
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

char *DirJoinPath(const char *folder, const char *name) {
    if (!folder || !*folder) return name ? strdup(name) : NULL;
    if (!name || !*name) return strdup(folder);
    size_t length = strlen(folder);
    char *result = malloc(length + strlen(name) + 2);
    if (!result) return NULL;
    if (folder[length - 1] == '\\' || folder[length - 1] == '/') {
        sprintf(result, "%s%s", folder, name);
    } else {
        sprintf(result, "%s\\%s", folder, name);
    }
    return result;
}

const char *DirBaseName(const char *path) {
    if (!path) return "";
    const char *backslash = strrchr(path, '\\');
    const char *slash = strrchr(path, '/');
    const char *last = backslash > slash ? backslash : slash;
    return last ? last + 1 : path;
}

char *DirParentPath(const char *path) {
    if (!path || !*path) return NULL;
    const char *backslash = strrchr(path, '\\');
    const char *slash = strrchr(path, '/');
    const char *last = backslash > slash ? backslash : slash;
    if (!last) return strdup(path);
    if (last == path) {
        // The path is in the root of a drive.
        return CopyFirst(path, 1);
    }
    return CopyFirst(path, (size_t)(last - path));
}

#else  // !_WIN32

int DirListRead(const char *path, struct DirEntry **out) {
    *out = NULL;
    if (!path || !*path) return -1;

    DIR *dir = opendir(path);
    if (!dir) {
        LOG_WARN("the folder '%s' could not be read", path);
        return -1;
    }
    struct DirEntry *entries = NULL;
    int count = 0, capacity = 0;
    struct dirent *item;
    while ((item = readdir(dir)) != NULL) {
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) continue;
        char *full = DirJoinPath(path, item->d_name);
        bool is_dir = false;
        if (full) {
            struct stat info;
            if (stat(full, &info) == 0) is_dir = S_ISDIR(info.st_mode);
            free(full);
        }
        AddEntry(&entries, &count, &capacity, item->d_name, is_dir);
    }
    closedir(dir);

    if (count > 1) qsort(entries, count, sizeof(struct DirEntry), CompareEntries);
    *out = entries;
    return count;
}

void DirListFree(struct DirEntry *entries, int count) {
    for (int i = 0; i < count; i++) free(entries[i].name);
    free(entries);
}

bool DirIsFolder(const char *path) {
    if (!path || !*path) return false;
    struct stat info;
    if (stat(path, &info) != 0) return false;
    return S_ISDIR(info.st_mode);
}

char *DirJoinPath(const char *folder, const char *name) {
    if (!folder || !*folder) return name ? strdup(name) : NULL;
    if (!name || !*name) return strdup(folder);
    size_t length = strlen(folder);
    char *result = malloc(length + strlen(name) + 2);
    if (!result) return NULL;
    if (folder[length - 1] == '/' || folder[length - 1] == '\\') {
        sprintf(result, "%s%s", folder, name);
    } else {
        sprintf(result, "%s/%s", folder, name);
    }
    return result;
}

const char *DirBaseName(const char *path) {
    if (!path) return "";
    const char *backslash = strrchr(path, '\\');
    const char *slash = strrchr(path, '/');
    const char *last = backslash > slash ? backslash : slash;
    return last ? last + 1 : path;
}

char *DirParentPath(const char *path) {
    if (!path || !*path) return NULL;
    const char *backslash = strrchr(path, '\\');
    const char *slash = strrchr(path, '/');
    const char *last = backslash > slash ? backslash : slash;
    if (!last) return strdup(path);
    if (last == path) {
        // The path is in the root.
        return strdup("/");
    }
    return CopyFirst(path, (size_t)(last - path));
}

#endif  // !_WIN32
