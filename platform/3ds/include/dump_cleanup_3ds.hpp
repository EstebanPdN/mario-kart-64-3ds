#pragma once
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mk64_3ds {
// Caller provides only the fixed diagnostic directory. Never follow a link
// into saves, game data or any other directory. Partial failures stay visible.
inline bool RemoveDumpContents(const char* directory, unsigned depth = 0) {
    if (depth > 32) return false;
    DIR* entries = opendir(directory);
    if (entries == nullptr) return errno == ENOENT;
    bool ok = true;
    while (dirent* entry = readdir(entries)) {
        if (!std::strcmp(entry->d_name, ".") || !std::strcmp(entry->d_name, "..")) continue;
        char path[1024];
        const int n = std::snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        if (n < 0 || static_cast<size_t>(n) >= sizeof(path)) { ok = false; continue; }
        struct stat info = {};
        if (lstat(path, &info) != 0) { ok = false; continue; }
        if (S_ISDIR(info.st_mode)) {
            if (!RemoveDumpContents(path, depth + 1) || rmdir(path) != 0) ok = false;
        } else if (unlink(path) != 0) {
            ok = false;
        }
    }
    if (closedir(entries) != 0) ok = false;
    return ok;
}
}
