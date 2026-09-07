#pragma once

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mk64_3ds {
inline unsigned DumpProgressPercent(std::uint32_t done, std::uint32_t total) {
    if (total == 0) return 0;
    if (done >= total) return 100;
    return static_cast<unsigned>(static_cast<std::uint64_t>(done) * 100U / total);
}

inline bool ParseDumpNumber(const char* text, std::uint32_t* number, const char** end) {
    if (text == nullptr || *text < '0' || *text > '9') return false;
    std::uint32_t value = 0;
    while (*text >= '0' && *text <= '9') {
        const unsigned digit = *text++ - '0';
        if (value > (UINT32_MAX - digit) / 10U) return false;
        value = value * 10U + digit;
    }
    *number = value;
    *end = text;
    return true;
}

// Recover from existing dumps. An empty dump collection starts a new sequence;
// the runtime log and filesystem metadata do not keep an old counter alive.
inline bool NextDumpNumber(const char* directory, const char* counterPath, std::uint32_t* next) {
    *next = 0;
    char counter[64] = {};
    if (FILE* file = std::fopen(counterPath, "rb")) {
        if (std::fgets(counter, sizeof(counter), file)) {
            const char* end = nullptr;
            std::uint32_t stored = 0;
            if (ParseDumpNumber(counter, &stored, &end) && (*end == '\n' || *end == '\0')) *next = stored;
        }
        std::fclose(file);
    }
    DIR* entries = opendir(directory);
    if (entries == nullptr) return false;
    bool hasDumps = false;
    while (dirent* entry = readdir(entries)) {
        if (std::strncmp(entry->d_name, "dump-", 5) == 0) hasDumps = true;
        std::uint32_t number;
        const char* end;
        if (!ParseDumpNumber(entry->d_name, &number, &end) || std::strncmp(end, "-dump-", 6) != 0) continue;
        hasDumps = true;
        if (number == UINT32_MAX) { closedir(entries); return false; }
        if (number >= *next) *next = number + 1U;
    }
    closedir(entries);
    if (!hasDumps) *next = 0;
    return *next != UINT32_MAX;
}

inline bool SaveDumpNumber(const char* counterPath, std::uint32_t next) {
    char temporary[256];
    const int n = std::snprintf(temporary, sizeof(temporary), "%s.tmp", counterPath);
    if (n < 0 || static_cast<size_t>(n) >= sizeof(temporary)) return false;
    FILE* file = std::fopen(temporary, "wb");
    if (file == nullptr) return false;
    bool ok = std::fprintf(file, "%lu\n", static_cast<unsigned long>(next)) > 0;
    if (std::fflush(file) != 0 || fsync(fileno(file)) != 0) ok = false;
    if (std::fclose(file) != 0) ok = false;
#ifdef __3DS__
    // libctru's FS rename does not replace an existing destination. The fully
    // flushed temporary is ready before unlinking; folder scanning recovers
    // the number if power is lost between these two directory operations.
    if (ok && std::remove(counterPath) != 0 && errno != ENOENT) ok = false;
#endif
    if (ok && std::rename(temporary, counterPath) == 0) return true;
    std::remove(temporary);
    return false;
}

// Called only while gameplay is paused. The counter remains outside the dump folder.
inline bool CreateNumberedDump(const char* directory, const char* counterPath,
                               const char* stamp, char* output, size_t outputSize,
                               bool* counterSaved) {
    *counterSaved = false;
    std::uint32_t next;
    if (!NextDumpNumber(directory, counterPath, &next)) return false;
    const int length = std::snprintf(output, outputSize, "%s/%03lu-dump-%s", directory,
                                     static_cast<unsigned long>(next), stamp);
    if (length < 0 || static_cast<size_t>(length) >= outputSize || mkdir(output, 0777) != 0) return false;
    *counterSaved = SaveDumpNumber(counterPath, next + 1U);
    return true;
}
}
