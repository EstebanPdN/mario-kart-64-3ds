#include "game_data_3ds.h"
#include "game_data_archive_3ds.hpp"
#include "install_progress_3ds.hpp"
#include "install_log_3ds.h"
#include "loading_image_3ds.h"

#include <3ds.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <exception>
#include <limits>
#include <malloc.h>
#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <vector>

#include "o2r_archive_reader.hpp"

#if defined(MK64_3DS_ON_DEVICE_EXTRACTOR)
bool Mk64Torch3DSBuildO2R(const char* rom, const char* sourceDir, const char* destinationDir,
                          const char* additionalFile, char* error, size_t errorSize);
#endif

namespace {
constexpr const char* kDataDir = "sdmc:/3ds/MK64";
constexpr const char* kPrimaryArchivePath = "sdmc:/3ds/MK64/mk64.o2r";
constexpr const char* kLegacyArchivePath = "sdmc:/3ds/mk64-3ds/mk64.o2r";
constexpr const char* kInstallerDir = "sdmc:/3ds/MK64/.mk64-3ds-installer";
constexpr const char* kExtractorSourceDir = "sdmc:/3ds/MK64/.mk64-3ds-installer/torch";
constexpr const char* kExtractorWorkDir = "sdmc:/3ds/MK64/.mk64-3ds-installer/work";
constexpr const char* kWorkArchivePath = "sdmc:/3ds/MK64/.mk64-3ds-installer/work/mk64.o2r";
constexpr const char* kInvalidArchiveDir = "sdmc:/3ds/MK64/.mk64-3ds-installer/invalid";
constexpr const char* kExtractorAdditionalFile = "meta/mods.toml";
constexpr const char* kRomfsExtractorSourceDir = "romfs:/torch";
constexpr const char* kExpectedSha1 = "579c48e211ae952530ffc8738709f078d5dd215e";
// The on-device ZIP writer stores roughly 54 MiB of payload and temporarily
// spools its central directory alongside the bundled extractor metadata.
// Keep enough margin for FAT allocation granularity and installer logs.
constexpr uint64_t kMinimumExtractionFreeBytes = 96ULL * 1024ULL * 1024ULL;
constexpr std::array<const char*, 3> kRomPaths = {
    "sdmc:/3ds/MK64/Mario Kart 64.z64",
    "sdmc:/3ds/MK64/mk64.z64",
    "sdmc:/3ds/MK64/baserom.us.z64",
};

struct Sha1Context {
    uint32_t state[5];
    uint64_t bitCount;
    uint8_t buffer[64];
};

struct CopyStats {
    uint32_t fileCount = 0;
    uint64_t byteCount = 0;
};

PrintConsole gBottomConsole{};
std::array<std::array<char, 76>, 22> gInstallConsoleLines{};
uint32_t gInstallConsoleLineCount = 0;
std::atomic<int> gInstallProgressPercent{0};
bool gInstallShellStateAvailable = false;
LightLock gInstallDisplayLock;
bool gInstallDisplayLockReady = false;
Thread gInstallLidWatcherThread = nullptr;
std::atomic<bool> gInstallLidWatcherRunning{false};
std::atomic<bool> gInstallLidClosed{false};
bool gInstallLidPollingFallback = false;

class InstallDisplayLockGuard {
public:
    InstallDisplayLockGuard() {
        if (gInstallDisplayLockReady) LightLock_Lock(&gInstallDisplayLock);
    }
    ~InstallDisplayLockGuard() {
        if (gInstallDisplayLockReady) LightLock_Unlock(&gInstallDisplayLock);
    }
};

void PushInstallConsoleLine(const char* format, ...) {
    char line[76] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    InstallDisplayLockGuard lock;

    if (gInstallConsoleLineCount < gInstallConsoleLines.size()) {
        std::snprintf(gInstallConsoleLines[gInstallConsoleLineCount++].data(), gInstallConsoleLines[0].size(), "%s", line);
        return;
    }

    for (size_t i = 1; i < gInstallConsoleLines.size(); ++i) {
        gInstallConsoleLines[i - 1] = gInstallConsoleLines[i];
    }
    std::snprintf(gInstallConsoleLines.back().data(), gInstallConsoleLines[0].size(), "%s", line);
}

uint16_t PackRgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void UnpackRgb565(uint16_t color, uint8_t* r, uint8_t* g, uint8_t* b) {
    const uint8_t rawR = static_cast<uint8_t>((color >> 11) & 0x1F);
    const uint8_t rawG = static_cast<uint8_t>((color >> 5) & 0x3F);
    const uint8_t rawB = static_cast<uint8_t>(color & 0x1F);
    *r = static_cast<uint8_t>((rawR << 3) | (rawR >> 2));
    *g = static_cast<uint8_t>((rawG << 2) | (rawG >> 4));
    *b = static_cast<uint8_t>((rawB << 3) | (rawB >> 2));
}

uint16_t BlendRgb565(uint16_t background, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha) {
    uint8_t bgR = 0;
    uint8_t bgG = 0;
    uint8_t bgB = 0;
    UnpackRgb565(background, &bgR, &bgG, &bgB);
    const uint16_t invAlpha = static_cast<uint16_t>(255U - alpha);
    const uint8_t outR = static_cast<uint8_t>((static_cast<uint16_t>(r) * alpha + bgR * invAlpha) / 255U);
    const uint8_t outG = static_cast<uint8_t>((static_cast<uint16_t>(g) * alpha + bgG * invAlpha) / 255U);
    const uint8_t outB = static_cast<uint8_t>((static_cast<uint16_t>(b) * alpha + bgB * invAlpha) / 255U);
    return PackRgb565(outR, outG, outB);
}

void DrawTopPixel(uint16_t* frameBuffer, int x, int y, uint16_t color) {
    if (frameBuffer == nullptr || x < 0 || x >= 400 || y < 0 || y >= 240) {
        return;
    }
    frameBuffer[static_cast<size_t>(x) * 240 + static_cast<size_t>(239 - y)] = color;
}

void DrawTopRect(uint16_t* frameBuffer, int x, int y, int width, int height, uint16_t color) {
    for (int py = y; py < y + height; ++py) {
        for (int px = x; px < x + width; ++px) {
            DrawTopPixel(frameBuffer, px, py, color);
        }
    }
}

std::array<uint8_t, 7> LoadingGlyphRows(char glyph) {
    switch (glyph) {
        case 'A': return { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 };
        case 'E': return { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F };
        case 'F': return { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 };
        case 'H': return { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 };
        case 'I': return { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F };
        case 'K': return { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 };
        case 'L': return { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F };
        case 'M': return { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 };
        case 'O': return { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E };
        case 'R': return { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 };
        case 'S': return { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E };
        case 'T': return { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 };
        case 'V': return { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 };
        case 'W': return { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A };
        case '.': return { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C };
        default: return { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    }
}

int LoadingTextWidth(const char* text, int scale) {
    int width = 0;
    for (const char* cursor = text; cursor != nullptr && *cursor != '\0'; ++cursor) {
        width += (*cursor == ' ') ? 4 * scale : 6 * scale;
    }
    return width == 0 ? 0 : width - scale;
}

void DrawLoadingGlyph(uint16_t* frameBuffer, char glyph, int x, int y, int scale, uint16_t color) {
    const std::array<uint8_t, 7> rows = LoadingGlyphRows(glyph);
    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
            if ((rows[static_cast<size_t>(row)] & (1U << (4 - column))) == 0) continue;
            DrawTopRect(frameBuffer, x + column * scale, y + row * scale, scale, scale, color);
        }
    }
}

void DrawLoadingText(uint16_t* frameBuffer, const char* text, int centerX, int y,
                     int scale, uint16_t color) {
    const int width = LoadingTextWidth(text, scale);
    int x = centerX - width / 2;
    for (const char* cursor = text; cursor != nullptr && *cursor != '\0'; ++cursor) {
        if (*cursor != ' ') {
            DrawLoadingGlyph(frameBuffer, *cursor, x + scale, y + scale, scale, PackRgb565(0, 0, 0));
            DrawLoadingGlyph(frameBuffer, *cursor, x, y, scale, color);
        }
        x += (*cursor == ' ') ? 4 * scale : 6 * scale;
    }
}

void DrawTopBlendedRect(uint16_t* frameBuffer, int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b,
                        uint8_t alpha) {
    for (int py = y; py < y + height; ++py) {
        for (int px = x; px < x + width; ++px) {
            if (px < 0 || px >= 400 || py < 0 || py >= 240) {
                continue;
            }
            const uint16_t background = kMk64LoadingImageRgb565[py * 400 + px];
            DrawTopPixel(frameBuffer, px, py, BlendRgb565(background, r, g, b, alpha));
        }
    }
}

void DrawLoadingTopScreen(int percent) {
    uint16_t frameBufferWidth = 0;
    uint16_t frameBufferHeight = 0;
    uint16_t* frameBuffer =
        reinterpret_cast<uint16_t*>(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &frameBufferWidth, &frameBufferHeight));
    (void) frameBufferWidth;
    (void) frameBufferHeight;
    if (frameBuffer == nullptr) {
        return;
    }

    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 400; ++x) {
            DrawTopPixel(frameBuffer, x, y, kMk64LoadingImageRgb565[y * 400 + x]);
        }
    }

    const int clampedPercent = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
    const int barX = 44;
    const int barY = 201;
    const int barW = 304;
    const int barH = 18;
    DrawTopBlendedRect(frameBuffer, barX - 4, barY - 4, barW + 8, barH + 8, 3, 5, 12, 150);
    DrawTopBlendedRect(frameBuffer, barX - 2, barY - 2, barW + 4, barH + 4, 235, 242, 252, 135);
    DrawTopBlendedRect(frameBuffer, barX, barY, barW, barH, 20, 27, 42, 155);
    DrawTopBlendedRect(frameBuffer, barX, barY, (barW * clampedPercent) / 100, barH, 38, 235, 95, 205);
    DrawTopBlendedRect(frameBuffer, barX, barY, barW, 2, 150, 255, 185, 150);
    DrawLoadingText(frameBuffer, clampedPercent >= 90 ? "ALMOST THERE" : "THIS WILL TAKE FOREVER...",
                    200, 181, 2, PackRgb565(255, 240, 48));
}

void PrepareInstallScreensLocked() {
    DrawLoadingTopScreen(gInstallProgressPercent.load(std::memory_order_relaxed));
    consoleSelect(&gBottomConsole);
    consoleClear();
    std::printf("Mario Kart 64 3DS\n\n");
    std::printf("Installer output\n");
    std::printf("----------------\n");
    for (uint32_t i = 0; i < gInstallConsoleLineCount; ++i) {
        std::printf("%s\n", gInstallConsoleLines[i].data());
    }
    std::printf("\nLog: /3ds/MK64/mk64-install.log\n");
    gfxFlushBuffers();
}

void ApplyInstallLidState(bool closed) {
    InstallDisplayLockGuard lock;
    if (closed) {
        // Publish closed first so a new redraw cannot begin while the panels
        // are being blanked.
        gInstallLidClosed.store(true, std::memory_order_release);
        GSPGPU_SetLcdForceBlack(1);
    } else {
        // Unblank before allowing redraws to resume.
        GSPGPU_SetLcdForceBlack(0);
        gInstallLidClosed.store(false, std::memory_order_release);
    }
}

bool PollInstallLidState() {
    bool closed = gInstallLidClosed.load(std::memory_order_acquire);
    if (!gInstallShellStateAvailable) return closed;

    u8 shellState = closed ? 0 : 1;
    if (R_SUCCEEDED(PTMU_GetShellState(&shellState))) {
        const bool observedClosed = shellState == 0;
        if (observedClosed != closed) ApplyInstallLidState(observedClosed);
        closed = observedClosed;
    }
    return closed;
}

int AdvanceInstallProgress(int requestedPercent) {
    const int desired = mk64_3ds::install_progress::ClampPercent(requestedPercent);
    int observed = gInstallProgressPercent.load(std::memory_order_relaxed);
    while (observed < desired) {
        if (gInstallProgressPercent.compare_exchange_weak(
                observed, desired, std::memory_order_relaxed, std::memory_order_relaxed)) {
            return desired;
        }
    }
    return observed;
}

void RedrawInstallScreens(int percent) {
    AdvanceInstallProgress(percent);
    if (gInstallLidPollingFallback) PollInstallLidState();
    if (gInstallLidClosed.load(std::memory_order_acquire)) return;
    {
        InstallDisplayLockGuard lock;
        if (gInstallLidClosed.load(std::memory_order_acquire)) return;
        PrepareInstallScreensLocked();
    }

    // The watcher must be able to publish a closed lid and blank the LCD even
    // if swap or VBlank blocks. Never hold its state lock across either call.
    if (gInstallLidClosed.load(std::memory_order_acquire)) return;
    gfxSwapBuffers();
    if (!gInstallLidClosed.load(std::memory_order_acquire)) gspWaitForVBlank();
}

void ExtractionLidWatcherMain(void*) {
    bool lastClosed = gInstallLidClosed.load(std::memory_order_acquire);
    while (gInstallLidWatcherRunning.load(std::memory_order_acquire)) {
        u8 shellState = lastClosed ? 0 : 1;
        if (R_SUCCEEDED(PTMU_GetShellState(&shellState))) {
            const bool closed = shellState == 0;
            if (closed != lastClosed) {
                ApplyInstallLidState(closed);
                // Unblanking exposes the most recently completed framebuffer
                // immediately. The extraction thread owns all drawing and
                // refreshes it again on its next progress callback.
                lastClosed = closed;
            }
        }
        svcSleepThread(50LL * 1000LL * 1000LL);
    }
}

void OnInstallLogLine(const char* message) {
    PushInstallConsoleLine("%s", message != nullptr ? message : "");

    int entries = 0;
    if (message != nullptr && std::sscanf(message, "O2R progress: %d entries", &entries) == 1) {
        const std::uint32_t completedEntries =
            entries > 0 ? static_cast<std::uint32_t>(entries) : 0;
        RedrawInstallScreens(mk64_3ds::install_progress::MapGeneratedEntries(completedEntries));
        return;
    }
    RedrawInstallScreens(gInstallProgressPercent.load(std::memory_order_relaxed));
}

struct ValidationProgressRange {
    int startPercent;
    int endPercent;
};

void OnArchiveValidationProgress(std::size_t completedEntries, std::size_t totalEntries,
                                 void* userData) {
    const auto* range = static_cast<const ValidationProgressRange*>(userData);
    if (range == nullptr) return;
    const std::size_t maxWork = std::numeric_limits<std::uint32_t>::max();
    const std::uint32_t completed = static_cast<std::uint32_t>(
        completedEntries < maxWork ? completedEntries : maxWork);
    const std::uint32_t total = static_cast<std::uint32_t>(
        totalEntries < maxWork ? totalEntries : maxWork);
    const int percent = mk64_3ds::install_progress::MapValidatedEntries(
        completed, total, range->startPercent, range->endPercent);
    if (percent > gInstallProgressPercent.load(std::memory_order_relaxed)) {
        RedrawInstallScreens(percent);
    }
}

bool FileExists(const char* path) {
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }
    std::fclose(file);
    return true;
}

bool DirectoryExists(const char* path) {
    struct stat info {};
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

bool MakeDirectory(const char* path) {
    return mkdir(path, 0777) == 0 || errno == EEXIST;
}

bool JoinPath(char* output, size_t outputSize, const char* base, const char* name) {
    const int written = std::snprintf(output, outputSize, "%s/%s", base, name);
    return written > 0 && static_cast<size_t>(written) < outputSize;
}

bool QuerySdFreeBytes(uint64_t* freeBytes) {
    if (freeBytes == nullptr) {
        errno = EINVAL;
        return false;
    }

    struct statvfs info {};
    if (statvfs(kDataDir, &info) != 0) {
        return false;
    }

    const uint64_t blockSize = info.f_frsize != 0 ? static_cast<uint64_t>(info.f_frsize)
                                                  : static_cast<uint64_t>(info.f_bsize);
    const uint64_t availableBlocks = static_cast<uint64_t>(info.f_bavail);
    if (blockSize == 0 || availableBlocks > std::numeric_limits<uint64_t>::max() / blockSize) {
        errno = EOVERFLOW;
        return false;
    }

    *freeBytes = availableBlocks * blockSize;
    return true;
}

void LogArchiveValidationFailure(const char* archiveLabel,
                                 const mk64_3ds::Mk64O2rValidationResult& validation) {
    if (validation.component != nullptr) {
        Mk64InstallLogWritef("%s validation failed: %s (%s).", archiveLabel,
                             mk64_3ds::Mk64O2rValidationMessage(validation.error), validation.component);
    } else {
        Mk64InstallLogWritef("%s validation failed: %s.", archiveLabel,
                             mk64_3ds::Mk64O2rValidationMessage(validation.error));
    }
}

bool PreserveInvalidArchive(const char* archivePath, const char* archiveLabel, const char* reason,
                            char* preservedPath, size_t preservedPathSize) {
    if (archivePath == nullptr || archiveLabel == nullptr || archiveLabel[0] == '\0' || !FileExists(archivePath)) {
        errno = EINVAL;
        return false;
    }
    if (!MakeDirectory(kInstallerDir) || !MakeDirectory(kInvalidArchiveDir)) {
        Mk64InstallLogWritef("Could not create the private invalid-archive directory; errno=%d.", errno);
        return false;
    }

    struct stat archiveInfo {};
    const uint64_t archiveBytes = stat(archivePath, &archiveInfo) == 0 && archiveInfo.st_size > 0
                                      ? static_cast<uint64_t>(archiveInfo.st_size)
                                      : 0;

    // Quarantine is diagnostic, not a retry mechanism. Keep only the newest
    // rejected archive for each class so repeated failures cannot silently
    // consume gigabytes of SD space and eventually defeat the next preflight.
    char candidate[768] = {};
    const int written = std::snprintf(candidate, sizeof(candidate), "%s/%s-latest.o2r",
                                      kInvalidArchiveDir, archiveLabel);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(candidate)) {
        errno = ENAMETOOLONG;
        return false;
    }
    if (FileExists(candidate) && std::remove(candidate) != 0) {
        Mk64InstallLogWritef("Could not replace the previous rejected O2R archive; errno=%d.", errno);
        return false;
    }
    if (rename(archivePath, candidate) != 0) {
        Mk64InstallLogWritef("Could not preserve the rejected O2R archive; errno=%d.", errno);
        return false;
    }

    if (preservedPath != nullptr && preservedPathSize > 0) {
        std::snprintf(preservedPath, preservedPathSize, "%s", candidate);
    }
    Mk64InstallLogWritef("Preserved the newest rejected O2R archive privately (%llu bytes, reason: %s).",
                         static_cast<unsigned long long>(archiveBytes),
                         reason != nullptr && reason[0] != '\0' ? reason : "validation failed");
    return true;
}

bool RemoveArchiveCentralDirectorySpool(const char* archivePath) {
    if (archivePath == nullptr || archivePath[0] == '\0') {
        errno = EINVAL;
        return false;
    }
    char spoolPath[768] = {};
    const int written = std::snprintf(spoolPath, sizeof(spoolPath), "%s.cd", archivePath);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(spoolPath)) {
        errno = ENAMETOOLONG;
        return false;
    }
    if (std::remove(spoolPath) == 0 || errno == ENOENT) {
        return true;
    }
    Mk64InstallLogWritef("Could not remove the abandoned O2R central-directory spool; errno=%d.",
                         errno);
    return false;
}

bool DiscardIncompleteArchive(const char* archivePath, const char* reason) {
    if (archivePath == nullptr || !FileExists(archivePath)) {
        return RemoveArchiveCentralDirectorySpool(archivePath);
    }
    struct stat archiveInfo {};
    const uint64_t archiveBytes = stat(archivePath, &archiveInfo) == 0 && archiveInfo.st_size > 0
                                      ? static_cast<uint64_t>(archiveInfo.st_size)
                                      : 0;
    if (std::remove(archivePath) != 0) {
        Mk64InstallLogWritef("Could not remove the non-resumable incomplete O2R archive; errno=%d.", errno);
        return false;
    }
    Mk64InstallLogWritef(
        "Removed a non-resumable incomplete O2R archive (%llu bytes, reason: %s).",
        static_cast<unsigned long long>(archiveBytes),
        reason != nullptr && reason[0] != '\0' ? reason : "validation failed");
    return RemoveArchiveCentralDirectorySpool(archivePath);
}

void CleanupLegacyIncompleteQuarantine() {
    // v0.18 could retain a new 50+ MiB partial after every failed retry. None
    // of those files has a usable ZIP directory or can be resumed, so reclaim
    // them before the current free-space preflight. Preserve other rejected
    // archives, which may still be useful for corruption diagnostics.
    constexpr std::array<const char*, 2> kLegacyPartialLabels = {
        "mk64-incomplete",
        "mk64-interrupted",
    };
    uint64_t removedBytes = 0;
    unsigned int removedFiles = 0;
    char candidate[768] = {};
    for (const char* label : kLegacyPartialLabels) {
        for (unsigned int index = 1; index <= 99; ++index) {
            const int written = std::snprintf(candidate, sizeof(candidate), "%s/%s-%02u.o2r",
                                              kInvalidArchiveDir, label, index);
            if (written <= 0 || static_cast<size_t>(written) >= sizeof(candidate) ||
                !FileExists(candidate)) {
                continue;
            }
            struct stat archiveInfo {};
            const uint64_t bytes = stat(candidate, &archiveInfo) == 0 && archiveInfo.st_size > 0
                                       ? static_cast<uint64_t>(archiveInfo.st_size)
                                       : 0;
            if (std::remove(candidate) == 0) {
                removedBytes += bytes;
                ++removedFiles;
            } else {
                Mk64InstallLogWritef("Could not remove legacy incomplete archive %s; errno=%d.",
                                     candidate, errno);
            }
        }
    }
    if (removedFiles != 0) {
        Mk64InstallLogWritef(
            "Removed %u legacy non-resumable partial archives and reclaimed %llu bytes.",
            removedFiles, static_cast<unsigned long long>(removedBytes));
    }
}

bool CopyFile(const char* source, const char* destination, CopyStats* stats) {
    FILE* input = std::fopen(source, "rb");
    if (input == nullptr) {
        Mk64InstallLogWritef("Extractor metadata input open failed: %s (errno=%d).", source, errno);
        return false;
    }
    FILE* output = std::fopen(destination, "wb");
    if (output == nullptr) {
        const int openError = errno;
        std::fclose(input);
        Mk64InstallLogWritef("Extractor metadata output open failed: %s (errno=%d).", destination, openError);
        errno = openError;
        return false;
    }

    std::array<uint8_t, 64 * 1024> buffer{};
    bool ok = true;
    while (!std::feof(input)) {
        const size_t bytesRead = std::fread(buffer.data(), 1, buffer.size(), input);
        if (bytesRead == 0) break;
        if (std::fwrite(buffer.data(), 1, bytesRead, output) != bytesRead) {
            Mk64InstallLogWritef("Extractor metadata write failed: %s (errno=%d).", destination, errno);
            ok = false;
            break;
        }
        stats->byteCount += bytesRead;
    }
    if (std::ferror(input) != 0) {
        Mk64InstallLogWritef("Extractor metadata read failed: %s (errno=%d).", source, errno);
        ok = false;
    }
    if (std::fclose(output) != 0) {
        Mk64InstallLogWritef("Extractor metadata close failed: %s (errno=%d).", destination, errno);
        ok = false;
    }
    if (std::fclose(input) != 0) {
        Mk64InstallLogWritef("Extractor metadata input close failed: %s (errno=%d).", source, errno);
        ok = false;
    }
    if (ok) ++stats->fileCount;
    return ok;
}

bool CopyDirectoryTree(const char* source, const char* destination, CopyStats* stats) {
    if (!MakeDirectory(destination)) {
        Mk64InstallLogWritef("Extractor metadata directory creation failed: %s (errno=%d).", destination, errno);
        return false;
    }
    DIR* directory = opendir(source);
    if (directory == nullptr) {
        Mk64InstallLogWritef("Extractor metadata directory open failed: %s (errno=%d).", source, errno);
        return false;
    }

    bool ok = true;
    while (ok) {
        dirent* entry = readdir(directory);
        if (entry == nullptr) break;
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;

        char sourcePath[768];
        char destinationPath[768];
        if (!JoinPath(sourcePath, sizeof(sourcePath), source, entry->d_name) ||
            !JoinPath(destinationPath, sizeof(destinationPath), destination, entry->d_name)) {
            Mk64InstallLogWritef("Extractor metadata path was too long below: %s.", source);
            ok = false;
            break;
        }

        struct stat info {};
        if (stat(sourcePath, &info) != 0) {
            Mk64InstallLogWritef("Extractor metadata stat failed: %s (errno=%d).", sourcePath, errno);
            ok = false;
        } else if (S_ISDIR(info.st_mode)) {
            ok = CopyDirectoryTree(sourcePath, destinationPath, stats);
        } else if (S_ISREG(info.st_mode)) {
            ok = CopyFile(sourcePath, destinationPath, stats);
        }
    }
    closedir(directory);
    return ok;
}

void SetResult(Mk64GameData3DSResult* result, Mk64GameData3DSStatus status, const char* archivePath,
               const char* message) {
    result->status = status;
    result->archivePath = archivePath;
    std::snprintf(result->message, sizeof(result->message), "%s", message);
}

void DrawProgress(const char* status, const char* detail, int percent) {
    const int displayedPercent = AdvanceInstallProgress(percent);

    // Route every dynamic bottom-screen line through the same writer as the
    // extractor diagnostics. This makes mk64-install.log a complete transcript
    // of the installation console, including progress and user-facing errors.
    Mk64InstallLogWritef("%3d%% %s", displayedPercent,
                         status != nullptr ? status : "");
    if (detail != nullptr && detail[0] != '\0') {
        Mk64InstallLogWritef("     %s", detail);
    }

    RedrawInstallScreens(displayedPercent);
}

void LogInstallerMemory(const char* phase) {
    const struct mallinfo heap = mallinfo();
    Mk64InstallLogWritef(
        "Installer memory at %s: heap arena=%lu allocated=%lu free=%lu releasable=%lu; linear free=%lu; "
        "application region unused=%lu/%lu.",
        phase != nullptr ? phase : "unknown stage", static_cast<unsigned long>(heap.arena),
        static_cast<unsigned long>(heap.uordblks), static_cast<unsigned long>(heap.fordblks),
        static_cast<unsigned long>(heap.keepcost), static_cast<unsigned long>(linearSpaceFree()),
        static_cast<unsigned long>(osGetMemRegionFree(MEMREGION_APPLICATION)),
        static_cast<unsigned long>(osGetMemRegionSize(MEMREGION_APPLICATION)));
}

class ExtractionAwakeGuard {
public:
    ExtractionAwakeGuard()
        : previousSleepAllowed_(aptIsSleepAllowed()), previousHomeAllowed_(aptIsHomeAllowed()),
          shellServiceReady_(R_SUCCEEDED(ptmuInit())) {
        aptSetSleepAllowed(false);
        aptSetHomeAllowed(false);
        gInstallShellStateAvailable = shellServiceReady_;
        gInstallLidPollingFallback = false;
        gInstallLidClosed.store(false, std::memory_order_release);

        // Arm the shell state and watcher before any installer log call. The
        // log callback redraws immediately, so a console already closed when
        // extraction begins must be known before that first callback.
        if (shellServiceReady_) {
            // Fail closed until the first successful PTMU sample. A transient
            // query failure may briefly keep the panels black, but can never
            // send a redraw into a closed-lid VBlank wait.
            ApplyInstallLidState(true);
            u8 shellState = 1;
            if (R_SUCCEEDED(PTMU_GetShellState(&shellState))) {
                ApplyInstallLidState(shellState == 0);
            }
            gInstallLidWatcherRunning.store(true, std::memory_order_release);
            gInstallLidWatcherThread = threadCreate(ExtractionLidWatcherMain, nullptr,
                                                     16 * 1024, 0x30, -1, false);
            if (gInstallLidWatcherThread == nullptr) {
                gInstallLidWatcherRunning.store(false, std::memory_order_release);
                gInstallLidPollingFallback = true;
            }
        }

        Mk64InstallLogWrite(
            "Unattended extraction enabled: lid-close sleep and HOME suspension are disabled until completion.");
        if (!shellServiceReady_) {
            Mk64InstallLogWrite("Shell-state service unavailable; extraction remains awake but screen refresh cannot be suppressed.");
        } else if (gInstallLidPollingFallback) {
            Mk64InstallLogWrite("Lid watcher unavailable; each installer redraw will poll the shell state instead.");
        }
    }

    ~ExtractionAwakeGuard() {
        Mk64InstallLogWrite("Unattended extraction ended; restoring the prior sleep and HOME policies.");
        gInstallLidWatcherRunning.store(false, std::memory_order_release);
        if (gInstallLidWatcherThread != nullptr) {
            threadJoin(gInstallLidWatcherThread, U64_MAX);
            threadFree(gInstallLidWatcherThread);
            gInstallLidWatcherThread = nullptr;
        }
        // Force-black is an extraction-only override. Clear it before PTMU and
        // normal APT sleep handling are restored, even if the lid remains shut.
        ApplyInstallLidState(false);
        gInstallLidPollingFallback = false;
        gInstallShellStateAvailable = false;
        if (shellServiceReady_) {
            ptmuExit();
        }
        aptSetHomeAllowed(previousHomeAllowed_);
        aptSetSleepAllowed(previousSleepAllowed_);
    }

    ExtractionAwakeGuard(const ExtractionAwakeGuard&) = delete;
    ExtractionAwakeGuard& operator=(const ExtractionAwakeGuard&) = delete;

private:
    bool previousSleepAllowed_;
    bool previousHomeAllowed_;
    bool shellServiceReady_;
};

bool SignalExtractionComplete() {
    const Result initResult = mcuHwcInit();
    if(R_FAILED(initResult)) {
        Mk64InstallLogWritef("Completion LED service unavailable (0x%08lX).",
                             static_cast<unsigned long>(initResult));
        return false;
    }

    InfoLedPattern pattern{};
    pattern.delay = 1;
    pattern.smoothing = 0;
    pattern.loopDelay = 0;
    pattern.blinkSpeed = 0;
    std::memset(pattern.bluePattern, 0xFF, sizeof(pattern.bluePattern));
    const Result ledResult = MCUHWC_SetInfoLedPattern(&pattern);
    mcuHwcExit();
    if(R_FAILED(ledResult)) {
        Mk64InstallLogWritef("Could not set the blue completion LED (0x%08lX).",
                             static_cast<unsigned long>(ledResult));
        return false;
    }

    Mk64InstallLogWrite("Blue notification LED enabled to signal that extraction completed successfully.");
    return true;
}

uint32_t Rol32(uint32_t value, uint32_t bits) {
    return (value << bits) | (value >> (32U - bits));
}

void Sha1Transform(Sha1Context* ctx, const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i) {
        w[i] = Rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];

    for (int i = 0; i < 80; ++i) {
        uint32_t f = 0;
        uint32_t k = 0;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        const uint32_t temp = Rol32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = Rol32(b, 30);
        b = a;
        a = temp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

void Sha1Init(Sha1Context* ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->bitCount = 0;
    std::memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void Sha1Update(Sha1Context* ctx, const uint8_t* data, size_t len) {
    size_t bufferIndex = static_cast<size_t>((ctx->bitCount / 8) % 64);
    ctx->bitCount += static_cast<uint64_t>(len) * 8U;

    size_t offset = 0;
    if (bufferIndex != 0) {
        const size_t needed = 64 - bufferIndex;
        if (len < needed) {
            std::memcpy(ctx->buffer + bufferIndex, data, len);
            return;
        }
        std::memcpy(ctx->buffer + bufferIndex, data, needed);
        Sha1Transform(ctx, ctx->buffer);
        offset += needed;
    }

    while (offset + 64 <= len) {
        Sha1Transform(ctx, data + offset);
        offset += 64;
    }

    if (offset < len) {
        std::memcpy(ctx->buffer, data + offset, len - offset);
    }
}

void Sha1Final(Sha1Context* ctx, uint8_t digest[20]) {
    uint8_t padding[64] = {0x80};
    uint8_t length[8];
    for (int i = 0; i < 8; ++i) {
        length[7 - i] = static_cast<uint8_t>((ctx->bitCount >> (i * 8)) & 0xFF);
    }

    const size_t bufferIndex = static_cast<size_t>((ctx->bitCount / 8) % 64);
    const size_t padLength = bufferIndex < 56 ? (56 - bufferIndex) : (120 - bufferIndex);
    Sha1Update(ctx, padding, padLength);
    Sha1Update(ctx, length, sizeof(length));

    for (int i = 0; i < 5; ++i) {
        digest[i * 4] = static_cast<uint8_t>((ctx->state[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = static_cast<uint8_t>((ctx->state[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = static_cast<uint8_t>((ctx->state[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = static_cast<uint8_t>(ctx->state[i] & 0xFF);
    }
}

bool Sha1File(const char* path, char outHex[41]) {
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    const std::uint32_t totalBytes =
        size > 0 && static_cast<unsigned long>(size) <= std::numeric_limits<std::uint32_t>::max()
            ? static_cast<std::uint32_t>(size)
            : 0;

    Sha1Context ctx;
    Sha1Init(&ctx);
    std::array<uint8_t, 64 * 1024> buffer{};
    long processed = 0;
    int lastPercent = -1;

    while (!std::feof(file)) {
        const size_t read = std::fread(buffer.data(), 1, buffer.size(), file);
        if (read == 0) {
            break;
        }
        Sha1Update(&ctx, buffer.data(), read);
        processed += static_cast<long>(read);
        const std::uint32_t processedBytes =
            processed > 0 && static_cast<unsigned long>(processed) <= std::numeric_limits<std::uint32_t>::max()
                ? static_cast<std::uint32_t>(processed)
                : totalBytes;
        const int percent = mk64_3ds::install_progress::MapRomHashBytes(processedBytes, totalBytes);
        if (percent != lastPercent) {
            DrawProgress("Checking ROM...", path, percent);
            lastPercent = percent;
        }
    }
    const bool readOk = std::ferror(file) == 0;
    std::fclose(file);
    if (!readOk) return false;

    uint8_t digest[20];
    Sha1Final(&ctx, digest);
    for (int i = 0; i < 20; ++i) {
        std::snprintf(outHex + i * 2, 3, "%02x", digest[i]);
    }
    outHex[40] = '\0';
    return true;
}

const char* FindRom() {
    for (const char* path : kRomPaths) {
        if (FileExists(path)) {
            return path;
        }
    }
    return nullptr;
}

bool InstallExtractorFiles(CopyStats* stats) {
    if (DirectoryExists(kExtractorSourceDir) && FileExists("sdmc:/3ds/MK64/.mk64-3ds-installer/torch/config.yml")) {
        Mk64InstallLogWrite("Refreshing existing extractor metadata from this installed build.");
    } else {
        Mk64InstallLogWrite("Installing extractor metadata from this installed build.");
    }

    if (!MakeDirectory(kInstallerDir)) return false;
    DrawProgress("Installing local extractor...", "Copying the bundled extractor files to the SD card.",
                 mk64_3ds::install_progress::kMetadataCopyStart);
    Mk64InstallLogWrite("Copying bundled Torch metadata from RomFS to SD card.");
    return CopyDirectoryTree(kRomfsExtractorSourceDir, kExtractorSourceDir, stats);
}

namespace {
mk64_3ds::Mk64O2rValidationResult ValidateArchive(const char* path,
                                                   bool verifyAllEntries = false,
                                                   mk64_3ds::Mk64O2rValidationProgressCallback
                                                       progressCallback = nullptr,
                                                   void* progressUserData = nullptr) {
#if defined(MK64_3DS_ON_DEVICE_EXTRACTOR)
    if (path == nullptr || path[0] == '\0') {
        mk64_3ds::Mk64O2rValidationResult validation{};
        validation.error = mk64_3ds::Mk64O2rValidationError::InvalidPath;
        return validation;
    }
    return mk64_3ds::ValidateMk64O2rArchive(path, verifyAllEntries, progressCallback,
                                            progressUserData);
#else
    (void)path;
    (void)verifyAllEntries;
    (void)progressCallback;
    (void)progressUserData;
    mk64_3ds::Mk64O2rValidationResult validation{};
    validation.error = mk64_3ds::Mk64O2rValidationError::Ok;
    return validation;
#endif
}
} // namespace

bool GenerateArchiveFromRom(const char* romPath, char* error, size_t errorSize) {
#if defined(MK64_3DS_ON_DEVICE_EXTRACTOR)
    DrawProgress("ROM verified.", "Preparing the local extractor on the SD card.",
                 mk64_3ds::install_progress::kPreparationStart);

    if (!MakeDirectory(kInstallerDir) || !MakeDirectory(kExtractorWorkDir)) {
        Mk64InstallLogWritef("Could not create temporary extraction directory; errno=%d.", errno);
        std::snprintf(error, errorSize, "Could not create the temporary extraction folder.");
        return false;
    }

    CleanupLegacyIncompleteQuarantine();
    if (!RemoveArchiveCentralDirectorySpool(kWorkArchivePath)) {
        std::snprintf(error, errorSize,
                      "An abandoned extraction spool could not be removed safely.");
        return false;
    }

    if (FileExists(kWorkArchivePath)) {
        Mk64InstallLogWrite("Found an archive left by an earlier interrupted installation; validating it.");
        DrawProgress("Checking interrupted extraction...",
                     "Reading the temporary archive before deciding whether to resume.",
                     mk64_3ds::install_progress::kPreparationStart);
        ValidationProgressRange recoveryProgress = {
            mk64_3ds::install_progress::kPreparationStart,
            mk64_3ds::install_progress::kRecoveryValidationEnd,
        };
        const mk64_3ds::Mk64O2rValidationResult staleValidation =
            ValidateArchive(kWorkArchivePath, true, OnArchiveValidationProgress,
                            &recoveryProgress);
        if (staleValidation.IsValid()) {
            if (FileExists(kPrimaryArchivePath)) {
                Mk64InstallLogWrite(
                    "The final O2R destination appeared while recovering a validated temporary archive.");
                std::snprintf(error, errorSize,
                              "The destination mk64.o2r appeared while recovering the temporary archive.");
                return false;
            }
            if (rename(kWorkArchivePath, kPrimaryArchivePath) == 0) {
                Mk64InstallLogWrite("Recovered and finalized the previously generated O2R archive.");
                DrawProgress("Game data recovered.", "mk64.o2r was restored in /3ds/MK64/.",
                             mk64_3ds::install_progress::kComplete);
                svcSleepThread(900LL * 1000LL * 1000LL);
                return true;
            }
            Mk64InstallLogWritef("A valid temporary O2R archive could not be finalized; errno=%d.", errno);
            std::snprintf(error, errorSize,
                          "A complete temporary archive could not be moved into /3ds/MK64/mk64.o2r.");
            return false;
        }

        LogArchiveValidationFailure("Interrupted temporary O2R archive", staleValidation);
        if (!DiscardIncompleteArchive(
                kWorkArchivePath,
                mk64_3ds::Mk64O2rValidationMessage(staleValidation.error))) {
            std::snprintf(error, errorSize,
                          "The interrupted archive is invalid and could not be removed safely.");
            return false;
        }
    }

    uint64_t sdFreeBytes = 0;
    DrawProgress("Checking SD card...", "Verifying free space for a complete extraction.",
                 mk64_3ds::install_progress::kRecoveryValidationEnd);
    if (!QuerySdFreeBytes(&sdFreeBytes)) {
        Mk64InstallLogWritef("SD free-space preflight failed; errno=%d.", errno);
        std::snprintf(error, errorSize, "Could not check the free space on the SD card.");
        DrawProgress("SD card check failed.", error,
                     mk64_3ds::install_progress::kRecoveryValidationEnd);
        svcSleepThread(1800LL * 1000LL * 1000LL);
        return false;
    }
    Mk64InstallLogWritef("SD free-space preflight: %llu bytes available; %llu bytes required.",
                         static_cast<unsigned long long>(sdFreeBytes),
                         static_cast<unsigned long long>(kMinimumExtractionFreeBytes));
    if (sdFreeBytes < kMinimumExtractionFreeBytes) {
        std::snprintf(error, errorSize,
                      "At least 96 MiB of free SD space is required to generate mk64.o2r safely.");
        DrawProgress("Not enough SD space.", "Free at least 96 MiB and try again.",
                     mk64_3ds::install_progress::kRecoveryValidationEnd);
        svcSleepThread(1800LL * 1000LL * 1000LL);
        return false;
    }

    Result romfsResult = romfsInit();
    Mk64InstallLogWritef("RomFS initialization returned 0x%08lX.", static_cast<unsigned long>(romfsResult));
    if (R_FAILED(romfsResult)) {
        std::snprintf(error, errorSize, "RomFS could not be opened from this install.");
        DrawProgress("Extractor metadata unavailable.", error,
                     mk64_3ds::install_progress::kRecoveryValidationEnd);
        svcSleepThread(1800LL * 1000LL * 1000LL);
        return false;
    }

    CopyStats copyStats{};
    const bool installed = InstallExtractorFiles(&copyStats);
    romfsExit();
    if (!installed) {
        Mk64InstallLogWritef("Extractor metadata copy failed after %lu files and %llu bytes; errno=%d.",
                             static_cast<unsigned long>(copyStats.fileCount),
                             static_cast<unsigned long long>(copyStats.byteCount), errno);
        std::snprintf(error, errorSize, "Could not copy the extractor files to /3ds/MK64/.");
        DrawProgress("Extractor install failed.", error,
                     mk64_3ds::install_progress::kMetadataCopyStart);
        svcSleepThread(1800LL * 1000LL * 1000LL);
        return false;
    }
    Mk64InstallLogWritef("Extractor metadata ready: %lu files, %llu bytes.",
                         static_cast<unsigned long>(copyStats.fileCount),
                         static_cast<unsigned long long>(copyStats.byteCount));

    DrawProgress("Extractor ready.", "Starting the bounded-memory O2R writer.",
                 mk64_3ds::install_progress::kMetadataCopyEnd);

    DrawProgress("Generating mk64.o2r...",
                 "Writing directly to SD. You may close the lid; extraction will continue.",
                 mk64_3ds::install_progress::kGenerationStart);
    Mk64InstallLogWrite("Starting O2R archive generation.");
    Mk64InstallLogWrite("Extractor execution mode: in-process Torch library (no child process or shell command).");
    LogInstallerMemory("before Torch setup");
    char originalDirectory[768] = {};
    if (getcwd(originalDirectory, sizeof(originalDirectory)) == nullptr) {
        Mk64InstallLogWritef("Could not read the current working directory; errno=%d.", errno);
        std::snprintf(error, errorSize, "Could not prepare the local extractor directory.");
        return false;
    }
    Mk64InstallLogWritef("Installer working directory before Torch: %s", originalDirectory);

    const char* romName = std::strrchr(romPath, '/');
    romName = romName == nullptr ? romPath : romName + 1;
    char relativeRomPath[192] = {};
    if (std::snprintf(relativeRomPath, sizeof(relativeRomPath), "../../%s", romName) <= 0 ||
        std::strlen(relativeRomPath) >= sizeof(relativeRomPath)) {
        Mk64InstallLogWrite("Could not build a relative ROM path for Torch.");
        std::snprintf(error, errorSize, "Could not prepare the ROM path for extraction.");
        return false;
    }

    if (chdir(kExtractorSourceDir) != 0) {
        Mk64InstallLogWritef("Could not enter the local Torch directory; errno=%d.", errno);
        std::snprintf(error, errorSize, "Could not open the local extractor directory.");
        return false;
    }
    char extractorDirectory[768] = {};
    if (getcwd(extractorDirectory, sizeof(extractorDirectory)) != nullptr) {
        Mk64InstallLogWritef("Installer working directory for Torch: %s", extractorDirectory);
    }
    Mk64InstallLogWrite("Torch uses relative paths to avoid 3DS filesystem handling of sdmc: paths.");
    bool ok = false;
    try {
        Mk64InstallLogWrite("Torch call entered.");
        ok = Mk64Torch3DSBuildO2R(relativeRomPath, ".", "../work", kExtractorAdditionalFile, error, errorSize);
    } catch (const std::exception& exception) {
        Mk64InstallLogWritef("Torch call escaped with an unexpected standard exception: %s.", exception.what());
        std::snprintf(error, errorSize, "The extractor stopped unexpectedly: %s", exception.what());
        ok = false;
    } catch (...) {
        Mk64InstallLogWrite("Torch call escaped with an unknown non-standard exception.");
        std::snprintf(error, errorSize, "The extractor stopped with an unknown error.");
        ok = false;
    }
    Mk64InstallLogWritef("Torch call returned success=%d.", ok ? 1 : 0);
    LogInstallerMemory("after Torch return");
    if (chdir(originalDirectory) != 0) {
        Mk64InstallLogWritef("Could not restore the working directory; errno=%d.", errno);
        if (ok) {
            std::snprintf(error, errorSize, "The extractor could not restore its working directory.");
            ok = false;
        }
    }

    if (!ok) {
        Mk64InstallLogWritef("O2R generation did not complete; Torch success=0, error=%s.",
                             error[0] != '\0' ? error : "(no extractor error)");
        LogInstallerMemory("O2R generation failure");
        if (FileExists(kWorkArchivePath)) {
            const mk64_3ds::Mk64O2rValidationResult partialValidation =
                ValidateArchive(kWorkArchivePath, true);
            if (partialValidation.IsValid()) {
                Mk64InstallLogWrite(
                    "The generated archive is complete but remains in the temporary folder for recovery on next launch.");
            } else {
                LogArchiveValidationFailure("Incomplete generated O2R archive", partialValidation);
                DiscardIncompleteArchive(
                    kWorkArchivePath,
                    mk64_3ds::Mk64O2rValidationMessage(partialValidation.error));
            }
        }
    } else if (!FileExists(kWorkArchivePath)) {
        Mk64InstallLogWrite("Torch reported success but did not create the temporary O2R archive.");
        std::snprintf(error, errorSize, "The extractor did not create a temporary mk64.o2r archive.");
    } else {
        Mk64InstallLogWrite(
            "Validating every generated O2R payload and CRC before finalizing it; this intentionally takes time.");
        DrawProgress("Validating generated data...",
                     "Reading every O2R payload and CRC back from the SD card.",
                     mk64_3ds::install_progress::kValidationStart);
        ValidationProgressRange generatedValidationProgress = {
            mk64_3ds::install_progress::kValidationStart,
            mk64_3ds::install_progress::kValidationEnd,
        };
        const mk64_3ds::Mk64O2rValidationResult generatedValidation =
            ValidateArchive(kWorkArchivePath, true, OnArchiveValidationProgress,
                            &generatedValidationProgress);
        if (!generatedValidation.IsValid()) {
            LogArchiveValidationFailure("Generated temporary O2R archive", generatedValidation);
            const bool preserved = PreserveInvalidArchive(
                kWorkArchivePath, "mk64-generated", mk64_3ds::Mk64O2rValidationMessage(generatedValidation.error),
                nullptr, 0);
            if (generatedValidation.component != nullptr) {
                std::snprintf(error, errorSize, "Generated game data failed validation: %s (%s).%s",
                              mk64_3ds::Mk64O2rValidationMessage(generatedValidation.error),
                              generatedValidation.component,
                              preserved ? " The rejected archive was preserved privately for diagnostics." : "");
            } else {
                std::snprintf(error, errorSize, "Generated game data failed validation: %s.%s",
                              mk64_3ds::Mk64O2rValidationMessage(generatedValidation.error),
                              preserved ? " The rejected archive was preserved privately for diagnostics." : "");
            }
        } else if (FileExists(kPrimaryArchivePath)) {
            Mk64InstallLogWrite("The final O2R destination unexpectedly appeared; the validated temporary archive was retained.");
            std::snprintf(error, errorSize,
                          "The destination mk64.o2r appeared during extraction; the validated temporary archive was kept.");
        } else if (rename(kWorkArchivePath, kPrimaryArchivePath) == 0) {
            Mk64InstallLogWritef("Generated O2R validation passed with %lu entries.",
                                 static_cast<unsigned long>(generatedValidation.entryCount));
            Mk64InstallLogWrite("Temporary O2R archive atomically moved into /3ds/MK64/mk64.o2r.");
            DrawProgress("Game data generated.", "mk64.o2r was created in /3ds/MK64/.",
                         mk64_3ds::install_progress::kComplete);
            svcSleepThread(900LL * 1000LL * 1000LL);
            return true;
        } else {
            Mk64InstallLogWritef("Validated O2R archive could not be atomically moved into place; errno=%d.", errno);
            std::snprintf(error, errorSize,
                          "The validated temporary O2R archive could not be moved into /3ds/MK64/.");
        }
    }

    if (error[0] == '\0') {
        std::snprintf(error, errorSize, "The temporary O2R archive could not be finalized on the SD card.");
    }
    const int failedPercent = gInstallProgressPercent.load(std::memory_order_relaxed);
    DrawProgress("O2R generation failed.", error,
                 failedPercent < mk64_3ds::install_progress::kGenerationStart
                     ? mk64_3ds::install_progress::kGenerationStart
                     : failedPercent);
    svcSleepThread(2500LL * 1000LL * 1000LL);
    return false;
#else
    (void)romPath;
    std::snprintf(error, errorSize, "This build was made without the on-device extractor.");
    DrawProgress("Extractor unavailable.", "This build was made without the on-device Torch pipeline.",
                 mk64_3ds::install_progress::kGenerationStart);
    svcSleepThread(1800LL * 1000LL * 1000LL);
    return false;
#endif
}
} // namespace

extern "C" Mk64GameData3DSResult Mk64GameData3DSEnsure(void) {
    Mk64GameData3DSResult result{};

    if (mkdir("sdmc:/3ds", 0777) != 0 && errno != EEXIST) {
        SetResult(&result, MK64_GAME_DATA_ERROR, nullptr, "Could not create sd:/3ds.");
        return result;
    }
    if (mkdir(kDataDir, 0777) != 0 && errno != EEXIST) {
        SetResult(&result, MK64_GAME_DATA_ERROR, nullptr, "Could not create sd:/3ds/MK64.");
        return result;
    }

    Mk64InstallLogBegin();
    Mk64InstallLogWrite("Installation check started.");

    if (FileExists(kPrimaryArchivePath)) {
        Mk64InstallLogWrite("Validating existing mk64.o2r archive.");
        const mk64_3ds::Mk64O2rValidationResult validation = ValidateArchive(kPrimaryArchivePath);
        if (!validation.IsValid()) {
            LogArchiveValidationFailure("Existing O2R archive", validation);
            if (!PreserveInvalidArchive(kPrimaryArchivePath, "mk64-primary",
                                        mk64_3ds::Mk64O2rValidationMessage(validation.error), nullptr, 0)) {
                Mk64InstallLogClose();
                SetResult(&result, MK64_GAME_DATA_ERROR, nullptr,
                          "Existing mk64.o2r is invalid and could not be preserved for diagnostics.");
                return result;
            }
            Mk64InstallLogWrite("Rejected O2R archive was quarantined privately; regeneration will use the legal ROM.");
        } else {
            Mk64InstallLogWritef("Existing O2R archive is valid (%lu entries).",
                                 static_cast<unsigned long>(validation.entryCount));
            Mk64InstallLogWrite("Existing O2R archive found; no installation work is needed.");
            Mk64InstallLogClose();
            SetResult(&result, MK64_GAME_DATA_READY, kPrimaryArchivePath, "Game data is ready.");
            return result;
        }
    }
    if (FileExists(kLegacyArchivePath)) {
        Mk64InstallLogWrite("Validating legacy mk64.o2r archive.");
        const mk64_3ds::Mk64O2rValidationResult validation = ValidateArchive(kLegacyArchivePath);
        if (!validation.IsValid()) {
            LogArchiveValidationFailure("Legacy O2R archive", validation);
            if (!PreserveInvalidArchive(kLegacyArchivePath, "mk64-legacy",
                                        mk64_3ds::Mk64O2rValidationMessage(validation.error), nullptr, 0)) {
                Mk64InstallLogClose();
                SetResult(&result, MK64_GAME_DATA_ERROR, nullptr,
                          "Legacy mk64.o2r is invalid and could not be preserved for diagnostics.");
                return result;
            }
            Mk64InstallLogWrite("Rejected legacy O2R archive was quarantined privately before regeneration.");
        } else {
            Mk64InstallLogWritef("Existing legacy O2R archive is valid (%lu entries).",
                                 static_cast<unsigned long>(validation.entryCount));
            Mk64InstallLogWrite("Existing legacy O2R archive found; no installation work is needed.");
            Mk64InstallLogClose();
            SetResult(&result, MK64_GAME_DATA_READY, kLegacyArchivePath, "Game data is ready.");
            return result;
        }
    }

    gfxInitDefault();
    gfxSetScreenFormat(GFX_TOP, GSP_RGB565_OES);
    gfxSetScreenFormat(GFX_BOTTOM, GSP_RGB565_OES);
    consoleInit(GFX_BOTTOM, &gBottomConsole);
    LightLock_Init(&gInstallDisplayLock);
    gInstallDisplayLockReady = true;
    gInstallConsoleLineCount = 0;
    gInstallProgressPercent.store(0, std::memory_order_relaxed);
    Mk64InstallLogSetCallback(OnInstallLogLine);
    Mk64InstallLogWrite("Bottom-screen installer console opened; subsequent screen lines are mirrored here.");
    LogInstallerMemory("installer console startup");
    DrawProgress("Looking for your ROM...", "Put it in /3ds/MK64/",
                 mk64_3ds::install_progress::kRomSearch);

    const char* romPath = FindRom();
    if (romPath == nullptr) {
        Mk64InstallLogWrite("No supported ROM filename was found in /3ds/MK64/.");
        Mk64InstallLogSetCallback(nullptr);
        gfxExit();
        Mk64InstallLogClose();
        SetResult(&result, MK64_GAME_DATA_MISSING_ROM, nullptr,
                  "Place your legally owned Mario Kart 64 USA ROM in sd:/3ds/MK64/.");
        return result;
    }

    char sha1[41];
    Mk64InstallLogWritef("ROM found at %s; validating SHA-1.", romPath);
    if (!Sha1File(romPath, sha1)) {
        Mk64InstallLogWritef("ROM validation read failed; errno=%d.", errno);
        Mk64InstallLogSetCallback(nullptr);
        gfxExit();
        Mk64InstallLogClose();
        SetResult(&result, MK64_GAME_DATA_ERROR, nullptr, "The ROM could not be read from sd:/3ds/MK64/.");
        return result;
    }

    if (std::strcmp(sha1, kExpectedSha1) != 0) {
        Mk64InstallLogWrite("ROM SHA-1 does not match the supported Mario Kart 64 USA revision.");
        Mk64InstallLogSetCallback(nullptr);
        gfxExit();
        Mk64InstallLogClose();
        SetResult(&result, MK64_GAME_DATA_BAD_ROM, nullptr,
                  "The ROM in sd:/3ds/MK64/ is not the supported Mario Kart 64 USA dump.");
        return result;
    }
    Mk64InstallLogWrite("ROM SHA-1 verified.");

    char extractionError[384] = {};
    bool extractionSucceeded = false;
    {
        ExtractionAwakeGuard awakeGuard;
        extractionSucceeded = GenerateArchiveFromRom(romPath, extractionError, sizeof(extractionError));
        if (extractionSucceeded) {
            SignalExtractionComplete();
        }
    }
    if (!extractionSucceeded) {
        Mk64InstallLogWritef("Installation failed: %s", extractionError);
        Mk64InstallLogSetCallback(nullptr);
        gfxExit();
        Mk64InstallLogClose();
        SetResult(&result, MK64_GAME_DATA_ERROR, nullptr, extractionError);
        return result;
    }
    Mk64InstallLogWrite("Installation completed successfully.");
    Mk64InstallLogSetCallback(nullptr);
    gfxExit();
    Mk64InstallLogClose();

    SetResult(&result, MK64_GAME_DATA_READY, kPrimaryArchivePath, "Game data is ready.");
    return result;
}
