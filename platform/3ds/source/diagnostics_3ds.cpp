#include "diagnostics_3ds.h"
#include "system_3ds.h"
#include "game_state_3ds.h"
#include "settings_3ds.h"
#include "performance_trace_3ds.hpp"
#include "interpolation_diagnostics_3ds.hpp"
#include "dump_sequence_3ds.hpp"
#include "dump_cleanup_3ds.hpp"
#include "bottom_ui_3ds.h"
#include "resource_runtime_3ds.h"
#include "ram_log_3ds.hpp"
#include "native_geometry_3ds.hpp"
#include "native_lighting_3ds.h"
#include "fast3d_reuse_3ds.hpp"
#include <citro3d.h>
extern "C" void Mk64Perf3DSLogFlush(uint64_t);

extern "C" { extern float gMk64DistanceFar3DS; extern uint32_t gMk64DistanceCulled3DS;
extern uint32_t gMk64DistanceFogDraws3DS; extern uint32_t gMk64DistanceFogSkipped3DS;
extern uint32_t gMk64DistanceFogBypassed3DS;
extern uint32_t gMk64DistanceFogBinds3DS; }
extern "C" unsigned int gMk64AudioMissingEnvelope3DS;

#include <3ds.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr const char* kGameDirectory = "sdmc:/3ds/MK64";
constexpr const char* kDumpDirectory = "sdmc:/3ds/MK64/dump";
constexpr const char* kRuntimeLog = "sdmc:/3ds/MK64/dump/runtime.log";
constexpr const char* kFatalLog = "sdmc:/3ds/MK64/dump/last-fatal.log";
constexpr size_t kMaxArenaDump = 32u * 1024u * 1024u;
constexpr size_t kMaxDisplayListDump = 4u * 1024u * 1024u;
constexpr size_t kRuntimeLogBufferSize = 64u * 1024u;
constexpr uint64_t kRuntimeFlushIntervalMilliseconds = 15000;

std::atomic<bool> sRunning{ false };
std::atomic<bool> sInputReady{ false };
std::atomic<bool> sDumpPaused{ false };
std::atomic<bool> sAptSuspended{ false };
std::atomic<unsigned> sDumpRequest{ 0 };
std::atomic<uint32_t> sKeysHeld{ 0 };
std::atomic<uint32_t> sKeysDownLatched{ 0 };
std::atomic<uint32_t> sKeysUpLatched{ 0 };
std::atomic<int> sCircleX{ 0 };
std::atomic<int> sCircleY{ 0 };
std::atomic<int> sCstickX{ 0 };
std::atomic<int> sCstickY{ 0 };
std::atomic<unsigned> sTouchX{ 0 };
std::atomic<unsigned> sTouchY{ 0 };
std::atomic<bool> sTouchHeld{ false };
std::atomic<uint32_t> sFrame{ 0 };
std::atomic<unsigned> sPresentation{ 0 };
std::atomic<size_t> sArchiveEntries{ 0 };
std::atomic<size_t> sLoadedResources{ 0 };
std::atomic<uintptr_t> sArenaBase{ 0 };
std::atomic<size_t> sArenaCapacity{ 0 };
std::atomic<uintptr_t> sDisplayListBase{ 0 };
std::atomic<size_t> sDisplayListSize{ 0 };
std::atomic<uintptr_t> sWatchdogCommand{ 0 };
std::atomic<uint32_t> sWatchdogWord0{ 0 };
std::atomic<uint32_t> sWatchdogWord1{ 0 };
std::atomic<size_t> sWatchdogCount{ 0 };
std::atomic<uint32_t> sLogLineCount{ 0 };
std::atomic<uint32_t> sLogFlushCount{ 0 };
std::atomic<uint32_t> sResourceUpdateCount{ 0 };
std::atomic<uint32_t> sCoursePrefetchDependencies{ 0 };
std::atomic<uint32_t> sCoursePrefetchLoaded{ 0 };
std::atomic<uint32_t> sCoursePrefetchBytes{ 0 };
std::atomic<uint32_t> sCoursePrefetchBudgetSkips{ 0 };
std::atomic<uint32_t> sCoursePrefetchUnavailable{ 0 };
std::atomic<uint32_t> sKartPrefetchAttempted{ 0 };
std::atomic<uint32_t> sKartPrefetchLoaded{ 0 };
std::atomic<uint32_t> sKartPrefetchBytes{ 0 };
std::atomic<uint32_t> sKartPrefetchDuplicates{ 0 };
std::atomic<uint32_t> sKartPrefetchUnavailable{ 0 };
std::atomic<uint32_t> sPrefetchUpdateSerial{ 0 };
std::atomic<uint32_t> sPrefetchLoggedSerial{ 0 };
std::atomic_flag sEmergencyWriteStarted = ATOMIC_FLAG_INIT;

LightLock sTextLock;
LightLock sLogLock;
char sStage[96] = "not-started";
char sLastResource[256] = "none";
Thread sThread = nullptr;
FILE* sLog = nullptr;
char sLogBuffer[kRuntimeLogBufferSize] = {};
mk64_3ds::RamLog<kRuntimeLogBufferSize> sPendingLog;
bool sBufferRuntimeLog = false; // protected by sLogLock
bool sIsNew3DS = false;
bool sSystemModelKnown = false;
u8 sSystemModel = 0xff;
uint64_t sStartTime = 0;

extern "C" void Mk64GameAudio3DSSetPaused(bool paused) __attribute__((weak));

enum DumpRequest : unsigned {
    kDumpRequestNone = 0,
    kDumpRequestSelect = 1,
    kDumpRequestBottomUi = 2,
    kDumpRequestFullMemory = 3,
    kDumpRequestClean = 4,
};

const char* DumpTriggerName(unsigned request) {
    switch (request) {
        case kDumpRequestSelect: return "SELECT";
        case kDumpRequestClean: return "CLEAN DUMPS";
        case kDumpRequestFullMemory: return "L+SELECT (process RAM)";
        case kDumpRequestBottomUi: return "BOTTOM UI";
        default: return "unknown";
    }
}

bool EnsureDirectory(const char* path) {
    if (mkdir(path, 0777) == 0) return true;
    if (errno != EEXIST) return false;
    struct stat info = {};
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

void CopyText(char* destination, size_t destinationSize, const char* source) {
    if (destination == nullptr || destinationSize == 0) return;
    std::snprintf(destination, destinationSize, "%s", source == nullptr ? "unknown" : source);
}

void ReadTextSnapshot(char* stage, size_t stageSize, char* resource, size_t resourceSize) {
    LightLock_Lock(&sTextLock);
    CopyText(stage, stageSize, sStage);
    CopyText(resource, resourceSize, sLastResource);
    LightLock_Unlock(&sTextLock);
}

void FlushLogLocked() {
    const uint64_t flushStart = mk64_3ds::PerformanceNow();
    if (sLog != nullptr) {
        sLogFlushCount.fetch_add(1, std::memory_order_relaxed);
        sPendingLog.Flush(sLog);
    }
    Mk64Perf3DSLogFlush(flushStart);
}

void LogLine(const char* prefix, const char* value, bool flush = false) {
    sLogLineCount.fetch_add(1, std::memory_order_relaxed);
    LightLock_Lock(&sLogLock);
    if (sLog != nullptr) {
        char line[1024];
        const int length = std::snprintf(line, sizeof(line), "%llu %s%s\n",
                     static_cast<unsigned long long>(osGetTime() - sStartTime),
                     prefix == nullptr ? "" : prefix, value == nullptr ? "unknown" : value);
        if (length > 0) {
            const size_t count = std::min(static_cast<size_t>(length), sizeof(line) - 1);
            line[count - 1] = '\n';
            sPendingLog.Append(line, count);
        }
        if (flush && !sBufferRuntimeLog) {
            FlushLogLocked();
        }
    }
    LightLock_Unlock(&sLogLock);
}

void WriteAll(int fd, const char* text) {
    if (fd < 0 || text == nullptr) return;
    size_t remaining = std::strlen(text);
    while (remaining != 0) {
        const ssize_t written = write(fd, text, remaining);
        if (written > 0) {
            text += written;
            remaining -= static_cast<size_t>(written);
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
}

void LogRuntimeHeartbeat() {
    char stage[sizeof(sStage)] = {};
    char resource[sizeof(sLastResource)] = {};
    ReadTextSnapshot(stage, sizeof(stage), resource, sizeof(resource));
    const struct mallinfo heap = mallinfo();
    char value[512] = {};
    std::snprintf(value, sizeof(value),
                  "frame=%lu resources=%lu heapFree=%d heapUsed=%d linearFree=%lu "
                  "stage=%s resource=%s",
                  static_cast<unsigned long>(sFrame.load(std::memory_order_relaxed)),
                  static_cast<unsigned long>(sLoadedResources.load(std::memory_order_relaxed)),
                  heap.fordblks, heap.uordblks,
                  static_cast<unsigned long>(linearSpaceFree()), stage, resource);
    LogLine("heartbeat: ", value, true);
}

void LogPrefetchSnapshotIfChanged() {
    const uint32_t serial = sPrefetchUpdateSerial.load(std::memory_order_acquire);
    if (serial == sPrefetchLoggedSerial.load(std::memory_order_relaxed)) {
        return;
    }
    char value[256] = {};
    std::snprintf(
        value, sizeof(value),
        "courseDependencies=%lu courseLoaded=%lu courseBytes=%lu courseCapped=%lu "
        "courseUnavailable=%lu kartAttempted=%lu kartLoaded=%lu kartBytes=%lu "
        "kartDuplicates=%lu kartUnavailable=%lu",
        static_cast<unsigned long>(sCoursePrefetchDependencies.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(sCoursePrefetchLoaded.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(sCoursePrefetchBytes.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(sCoursePrefetchBudgetSkips.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(sCoursePrefetchUnavailable.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(sKartPrefetchAttempted.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(sKartPrefetchLoaded.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(sKartPrefetchBytes.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(sKartPrefetchDuplicates.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(sKartPrefetchUnavailable.load(std::memory_order_relaxed)));
    LogLine("prefetch: ", value);
    sPrefetchLoggedSerial.store(serial, std::memory_order_release);
}

bool IsReadableRange(uintptr_t address, size_t size) {
    if (address == 0 || size == 0 || size > UINT32_MAX || address > UINT32_MAX ||
        address + size < address || address + size > UINT32_MAX) {
        return false;
    }
    const uintptr_t end = address + size;
    while (address < end) {
        MemInfo info = {};
        PageInfo page = {};
        if (R_FAILED(svcQueryMemory(&info, &page, static_cast<u32>(address))) ||
            (info.perm & MEMPERM_READ) == 0 || info.size == 0) {
            return false;
        }
        const uintptr_t regionEnd = static_cast<uintptr_t>(info.base_addr) + info.size;
        if (regionEnd <= address) return false;
        address = std::min(end, regionEnd);
    }
    return true;
}

void ShowDumpProgress(const char* label, size_t done, size_t total);
const char* sDumpProgressLabel = nullptr;
size_t sDumpProgressDone = 0, sDumpProgressTotal = 0;

bool WriteBlob(const char* path, const void* data, size_t size) {
    if (path == nullptr || !IsReadableRange(reinterpret_cast<uintptr_t>(data), size)) return false;
    FILE* file = std::fopen(path, "wb");
    if (file == nullptr) return false;
    bool ok = true;
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (size_t offset = 0; offset < size && ok;) {
        const size_t chunk = std::min<size_t>(256 * 1024, size - offset);
        ok = std::fwrite(bytes + offset, 1, chunk, file) == chunk;
        offset += chunk;
        if (sDumpProgressLabel != nullptr) {
            sDumpProgressDone += chunk;
            ShowDumpProgress(sDumpProgressLabel, sDumpProgressDone, sDumpProgressTotal);
        }
    }
    if (std::fclose(file) != 0) ok = false;
    if (!ok) {
        std::remove(path);
    }
    return ok;
}

void WriteU16(FILE* file, uint16_t value) {
    std::fputc(static_cast<int>(value & 0xffu), file);
    std::fputc(static_cast<int>((value >> 8) & 0xffu), file);
}

void WriteU32(FILE* file, uint32_t value) {
    WriteU16(file, static_cast<uint16_t>(value & 0xffffu));
    WriteU16(file, static_cast<uint16_t>((value >> 16) & 0xffffu));
}

const uint8_t* ReadFramebufferPixel(const uint8_t* framebuffer, uint16_t framebufferWidth,
                                    uint16_t framebufferHeight, uint16_t x, uint16_t y,
                                    bool rotate3dsPhysical) {
    (void) framebufferHeight;
    if (rotate3dsPhysical) {
        const uint16_t sourceX = static_cast<uint16_t>(framebufferWidth - 1u - y);
        const uint16_t sourceY = x;
        return framebuffer + (static_cast<size_t>(sourceY) * framebufferWidth + sourceX) * 3u;
    }
    return framebuffer + (static_cast<size_t>(y) * framebufferWidth + x) * 3u;
}

uint8_t* WriteFramebufferPixel(uint8_t* framebuffer, uint16_t framebufferWidth,
                               uint16_t framebufferHeight, uint16_t x, uint16_t y,
                               bool rotate3dsPhysical) {
    return const_cast<uint8_t*>(ReadFramebufferPixel(framebuffer, framebufferWidth, framebufferHeight,
                                                     x, y, rotate3dsPhysical));
}

bool WriteFramebufferBmp(const char* path, const uint8_t* framebuffer, uint16_t framebufferWidth,
                         uint16_t framebufferHeight) {
    if (path == nullptr || framebuffer == nullptr || framebufferWidth == 0 ||
        framebufferHeight == 0) {
        return false;
    }

    const bool rotate3dsPhysical = framebufferWidth == 240u &&
        (framebufferHeight == 800u || framebufferHeight == 400u || framebufferHeight == 320u);
    const uint16_t outputWidth = rotate3dsPhysical ? framebufferHeight : framebufferWidth;
    const uint16_t outputHeight = rotate3dsPhysical ? framebufferWidth : framebufferHeight;
    const uint32_t rowBytes = static_cast<uint32_t>(outputWidth) * 3u;
    const uint32_t padding = (4u - (rowBytes & 3u)) & 3u;
    const uint32_t imageBytes = (rowBytes + padding) * outputHeight;
    const uint32_t fileBytes = 14u + 40u + imageBytes;

    FILE* file = std::fopen(path, "wb");
    if (file == nullptr) return false;

    std::fputc('B', file);
    std::fputc('M', file);
    WriteU32(file, fileBytes);
    WriteU16(file, 0);
    WriteU16(file, 0);
    WriteU32(file, 14u + 40u);
    WriteU32(file, 40u);
    WriteU32(file, outputWidth);
    WriteU32(file, outputHeight);
    WriteU16(file, 1);
    WriteU16(file, 24);
    WriteU32(file, 0);
    WriteU32(file, imageBytes);
    WriteU32(file, 0);
    WriteU32(file, 0);
    WriteU32(file, 0);
    WriteU32(file, 0);

    bool ok = true;
    for (int y = static_cast<int>(outputHeight) - 1; y >= 0 && ok; --y) {
        for (uint16_t x = 0; x < outputWidth; ++x) {
            const uint8_t* pixel =
                ReadFramebufferPixel(framebuffer, framebufferWidth, framebufferHeight, x,
                                     static_cast<uint16_t>(y), rotate3dsPhysical);
            if (std::fwrite(pixel, 1, 3, file) != 3) {
                ok = false;
                break;
            }
        }
        for (uint32_t p = 0; p < padding && ok; ++p) {
            ok = std::fputc(0, file) != EOF;
        }
    }

    if (std::fclose(file) != 0) ok = false;
    if (!ok) std::remove(path);
    return ok;
}

void ShowDumpProgress(const char* label, size_t done, size_t total) {
    static uint64_t lastUpdate = 0;
    static const char* lastLabel = nullptr;
    const uint64_t now = osGetTime();
    if (label == lastLabel && done != total && now - lastUpdate < 100) return;
    lastLabel = label; lastUpdate = now;
    Mk64BottomUI3DSShowProgress("WRITING DUMP", label, mk64_3ds::DumpProgressPercent(done, total));
}

void ShowDumpSavedOverlay() {
    Mk64BottomUI3DSShowProgress("DUMP SAVED", "READY TO CONTINUE", 100);
    for (unsigned i = 0; i < 45; ++i) gspWaitForVBlank();
}


void WriteScreenCapture(FILE* manifest, const char* directory, gfxScreen_t screen,
                        gfx3dSide_t side, const char* name) {
    uint16_t width = 0;
    uint16_t height = 0;
    const uint8_t* framebuffer = gfxGetFramebuffer(screen, side, &width, &height);
    if (framebuffer == nullptr || width == 0 || height == 0) {
        if (manifest != nullptr) std::fprintf(manifest, "%s: unavailable\n", name);
        return;
    }

    char path[256] = {};
    std::snprintf(path, sizeof(path), "%s/%s-screen.rgb", directory, name);
    const size_t rawBytes = static_cast<size_t>(width) * height * 3u;
    const bool rawOk = WriteBlob(path, framebuffer, rawBytes);

    std::snprintf(path, sizeof(path), "%s/%s-screen.bmp", directory, name);
    const bool bmpOk = WriteFramebufferBmp(path, framebuffer, width, height);

    if (manifest != nullptr) {
        const bool rotated = width == 240u && (height == 800u || height == 400u || height == 320u);
        std::fprintf(manifest,
                     "%s: framebuffer=%ux%u rgb8/bgr8 bytes=%lu raw=%s bmp=%s bmpLogical=%ux%u\n",
                     name, width, height, static_cast<unsigned long>(rawBytes),
                     rawOk ? "ok" : "failed", bmpOk ? "ok" : "failed",
                     rotated ? height : width, rotated ? width : height);
    }
}

void WriteScreenCaptures(const char* directory) {
    char path[256] = {};
    std::snprintf(path, sizeof(path), "%s/screens.txt", directory);
    FILE* manifest = std::fopen(path, "wb");
    if (manifest != nullptr) {
        std::fprintf(manifest, "Physical 3DS framebuffer capture\n");
        std::fprintf(manifest, "Source: gfxGetFramebuffer, not renderer readback\n");
        std::fprintf(manifest, "Raw files preserve framebuffer memory exactly; BMP files rotate the native 3DS 240x800/240x400/240x320 layout into visible screen orientation.\n\n");
    }

    WriteScreenCapture(manifest, directory, GFX_TOP, GFX_LEFT, "top");
    WriteScreenCapture(manifest, directory, GFX_BOTTOM, GFX_LEFT, "bottom");

    if (manifest != nullptr) std::fclose(manifest);
}

void MakeTimestamp(char* output, size_t outputSize) {
    const time_t now = time(nullptr);
    const struct tm* local = now > 0 ? localtime(&now) : nullptr;
    if (local != nullptr && std::strftime(output, outputSize, "%Y%m%d-%H%M%S", local) != 0) return;
    std::snprintf(output, outputSize, "tick-%llu", static_cast<unsigned long long>(osGetTime()));
}

bool CreateSessionDirectory(char* output, size_t outputSize) {
    if (!EnsureDirectory(kGameDirectory) || !EnsureDirectory(kDumpDirectory)) return false;
    char stamp[40] = {};
    MakeTimestamp(stamp, sizeof(stamp));
    bool saved = false;
    const bool ok = mk64_3ds::CreateNumberedDump(kDumpDirectory,
        "sdmc:/3ds/MK64/dump-sequence.txt", stamp, output, outputSize, &saved);
    if (ok && !saved) LogLine("dump warning: ", "sequence counter could not be persisted; keep numbered folders on SD", true);
    return ok;
}

void WriteMemoryMap(FILE* file) {
    std::fprintf(file, "\n[Memory map]\n");
    uintptr_t address = 0;
    for (unsigned region = 0; region < 256 && address < 0x40000000u; ++region) {
        MemInfo info = {};
        PageInfo page = {};
        if (R_FAILED(svcQueryMemory(&info, &page, static_cast<u32>(address))) || info.size == 0) break;
        std::fprintf(file, "0x%08lX-0x%08lX size=%lu state=%lu perm=%c%c%c page=0x%08lX\n",
                     static_cast<unsigned long>(info.base_addr),
                     static_cast<unsigned long>(info.base_addr + info.size),
                     static_cast<unsigned long>(info.size), static_cast<unsigned long>(info.state),
                     (info.perm & MEMPERM_READ) ? 'r' : '-', (info.perm & MEMPERM_WRITE) ? 'w' : '-',
                     (info.perm & MEMPERM_EXECUTE) ? 'x' : '-', static_cast<unsigned long>(page.flags));
        const uintptr_t next = static_cast<uintptr_t>(info.base_addr) + info.size;
        if (next <= address) break;
        address = next;
    }
}

void CopyRuntimeLog(const char* directory) {
    LightLock_Lock(&sLogLock);
    FlushLogLocked();
    LightLock_Unlock(&sLogLock);

    FILE* source = std::fopen(kRuntimeLog, "rb");
    if (source == nullptr) return;
    char path[256] = {};
    std::snprintf(path, sizeof(path), "%s/runtime.log", directory);
    FILE* destination = std::fopen(path, "wb");
    if (destination == nullptr) {
        std::fclose(source);
        return;
    }
    char buffer[4096];
    size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), source)) != 0) {
        if (std::fwrite(buffer, 1, count, destination) != count) break;
    }
    std::fclose(destination);
    std::fclose(source);
}

// Capture readable application-owned mappings, including the ordinary heap,
// linear heap and stacks. Never read MMIO, VRAM or service shared mappings.
// This is a sequential process snapshot, not a kernel/all-process RAM dump.
void WriteProcessMemory(const char* directory) {
    char path[256];
    std::snprintf(path, sizeof(path), "%s/memory", directory);
    if (!EnsureDirectory(path)) return;
    std::snprintf(path, sizeof(path), "%s/memory/manifest.txt", directory);
    FILE* manifest = std::fopen(path, "wb");
    if (manifest == nullptr) return;
    std::fputs("Readable application RAM; game and audio paused, GPU submissions drained.\n"
               "Sequential capture: diagnostics/stdio and the active stack can change during writing.\n"
               "Excludes unmapped/reserved memory, MMIO, VRAM and service shared memory.\n", manifest);
    // Progress submissions are drained before each copy chunk. Game/audio
    // remain paused; diagnostic UI buffers may change in this sequential snapshot.
    sDumpProgressLabel = "PROCESS RAM";
    sDumpProgressDone = sDumpProgressTotal = 0;
    uintptr_t address = 0;
    for (unsigned region = 0; region < 256 && address < 0x40000000u; ++region) {
        MemInfo info = {}; PageInfo page = {};
        if (R_FAILED(svcQueryMemory(&info, &page, static_cast<u32>(address))) || info.size == 0) break;
        const bool application = info.state == MEMSTATE_CODE || info.state == MEMSTATE_PRIVATE ||
            info.state == MEMSTATE_CONTINUOUS || info.state == MEMSTATE_ALIASED ||
            info.state == MEMSTATE_ALIAS || info.state == MEMSTATE_ALIASCODE || info.state == MEMSTATE_LOCKED;
        if (application && (info.perm & MEMPERM_READ)) sDumpProgressTotal += info.size;
        const uintptr_t next = static_cast<uintptr_t>(info.base_addr) + info.size;
        if (next <= address) break;
        address = next;
    }
    ShowDumpProgress(sDumpProgressLabel, 0, sDumpProgressTotal);
    address = 0;
    for (unsigned region = 0; region < 256 && address < 0x40000000u; ++region) {
        MemInfo info = {}; PageInfo page = {};
        if (R_FAILED(svcQueryMemory(&info, &page, static_cast<u32>(address))) || info.size == 0) break;
        const uintptr_t next = static_cast<uintptr_t>(info.base_addr) + info.size;
        if (next <= address) break;
        const bool application = info.state == MEMSTATE_CODE || info.state == MEMSTATE_PRIVATE ||
            info.state == MEMSTATE_CONTINUOUS || info.state == MEMSTATE_ALIASED ||
            info.state == MEMSTATE_ALIAS || info.state == MEMSTATE_ALIASCODE || info.state == MEMSTATE_LOCKED;
        const bool include = application && (info.perm & MEMPERM_READ) != 0;
        bool ok = false;
        if (include) {
            std::snprintf(path, sizeof(path), "%s/memory/%08lX.bin", directory,
                          static_cast<unsigned long>(info.base_addr));
            ok = WriteBlob(path, reinterpret_cast<const void*>(info.base_addr), info.size);
        }
        std::fprintf(manifest, "%08lX size=%lu state=%lu perm=%lu %s\n",
            static_cast<unsigned long>(info.base_addr), static_cast<unsigned long>(info.size),
            static_cast<unsigned long>(info.state), static_cast<unsigned long>(info.perm),
            !include ? "excluded" : ok ? "saved" : "FAILED");
        address = next;
        if (include && !ok) break; // Preserve an explicit partial manifest on SD failure.
    }
    sDumpProgressLabel = nullptr;
    std::fclose(manifest);
}

void WriteQuickDump(const char* trigger, bool fullMemory) {
    char directory[192] = {};
    if (!CreateSessionDirectory(directory, sizeof(directory))) {
        LogLine("dump failed: ", "could not create session directory", true);
        return;
    }

    // Capture the game image before painting the paused write-progress screen.
    WriteScreenCaptures(directory);
    ShowDumpProgress("METADATA", 0, 1);
    char stage[96] = {};
    char resource[256] = {};
    ReadTextSnapshot(stage, sizeof(stage), resource, sizeof(resource));

    char path[256] = {};
    std::snprintf(path, sizeof(path), "%s/info.txt", directory);
    FILE* info = std::fopen(path, "wb");
    if (info != nullptr) {
        const struct mallinfo heap = mallinfo();
        uintptr_t workerSp = 0;
        __asm__ volatile("mov %0, sp" : "=r"(workerSp));
        s32 priority = -1;
        svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
        std::fprintf(info, "Mario Kart 64 3DS diagnostic dump\n");
        std::fprintf(info, "Build: %s\n", MK64_3DS_VERSION);
        std::fprintf(info, "Archive resident / bytes / physical reads / physical bytes: %s / %llu / %llu / %llu\n",
                     Mk64Resource3DSIsResident() ? "yes (SD handle closed)" : "no",
                     static_cast<unsigned long long>(Mk64Resource3DSArchiveBytes()),
                     static_cast<unsigned long long>(Mk64Resource3DSPhysicalReadCalls()),
                     static_cast<unsigned long long>(Mk64Resource3DSPhysicalReadBytes()));
        Mk64BottomUIGameState3DS game = {};
        Mk64GameState3DSGetBottomUISnapshot(&game);
        std::fprintf(info, "Game state / mode / track index / name / lap / timer: %ld / %ld / %lu / %s / %d / %.3f\n",
            static_cast<long>(game.gameState), static_cast<long>(game.gameMode),
            static_cast<unsigned long>(game.trackIndex), game.trackName ? game.trackName : "unknown",
            game.currentLap, game.courseTimerSeconds);
        std::fprintf(info, "Full process RAM requested: %s\n", fullMemory ? "yes" : "no");
        std::fprintf(info, "Trigger: %s\n", trigger == nullptr ? "unknown" : trigger);
        std::fprintf(info, "Uptime ms: %llu\n",
                     static_cast<unsigned long long>(osGetTime() - sStartTime));
        std::fprintf(info, "Game/audio paused during dump: yes\n");
        std::fprintf(info, "Stage: %s\n", stage);
        std::fprintf(info, "Last resource: %s\n", resource);
        std::fprintf(info, "Archive entries / loaded resources: %lu / %lu\n",
                     static_cast<unsigned long>(sArchiveEntries.load(std::memory_order_relaxed)),
                     static_cast<unsigned long>(sLoadedResources.load(std::memory_order_relaxed)));
        std::fprintf(info, "Buffered log lines / explicit flushes / resource updates: %lu / %lu / %lu\n",
                     static_cast<unsigned long>(sLogLineCount.load(std::memory_order_relaxed)),
                     static_cast<unsigned long>(sLogFlushCount.load(std::memory_order_relaxed)),
                     static_cast<unsigned long>(sResourceUpdateCount.load(std::memory_order_relaxed)));
        std::fprintf(
            info,
            "Course prefetch dependencies / loaded / bytes / capped / unavailable: %lu / %lu / %lu / %lu / %lu\n",
            static_cast<unsigned long>(sCoursePrefetchDependencies.load(std::memory_order_relaxed)),
            static_cast<unsigned long>(sCoursePrefetchLoaded.load(std::memory_order_relaxed)),
            static_cast<unsigned long>(sCoursePrefetchBytes.load(std::memory_order_relaxed)),
            static_cast<unsigned long>(sCoursePrefetchBudgetSkips.load(std::memory_order_relaxed)),
            static_cast<unsigned long>(sCoursePrefetchUnavailable.load(std::memory_order_relaxed)));
        std::fprintf(
            info,
            "Kart prefetch attempted / loaded / bytes / duplicates / unavailable: %lu / %lu / %lu / %lu / %lu\n",
            static_cast<unsigned long>(sKartPrefetchAttempted.load(std::memory_order_relaxed)),
            static_cast<unsigned long>(sKartPrefetchLoaded.load(std::memory_order_relaxed)),
            static_cast<unsigned long>(sKartPrefetchBytes.load(std::memory_order_relaxed)),
            static_cast<unsigned long>(sKartPrefetchDuplicates.load(std::memory_order_relaxed)),
            static_cast<unsigned long>(sKartPrefetchUnavailable.load(std::memory_order_relaxed)));
        std::fprintf(info, "Frame / presentation: %lu / %u\n",
                     static_cast<unsigned long>(sFrame.load(std::memory_order_relaxed)),
                     sPresentation.load(std::memory_order_relaxed));
        std::fprintf(info, "Keys held: 0x%08lX\n",
                     static_cast<unsigned long>(sKeysHeld.load(std::memory_order_relaxed)));
        std::fprintf(info, "Model: %s\n", sIsNew3DS ? "New 3DS" : "Old 3DS / unknown");
        std::fprintf(info, "Render scale / distance preset / HUD layout: %u / %d / %d\n",
                     Mk64Settings3DSGetRenderScalePercent(), Mk64Settings3DSGetRenderDistance(), Mk64Settings3DSGetHudLayout());
        std::fprintf(info, "Distance cutoff / culled triangles / fog states / fog stage overflow: %.1f / %lu / %lu / %lu\n",
                     gMk64DistanceFar3DS, static_cast<unsigned long>(gMk64DistanceCulled3DS),
                     static_cast<unsigned long>(gMk64DistanceFogDraws3DS), static_cast<unsigned long>(gMk64DistanceFogSkipped3DS));
        std::fprintf(info, "Distance fog near batches bypassed / texture binds: %lu / %lu\n",
                     static_cast<unsigned long>(gMk64DistanceFogBypassed3DS),
                     static_cast<unsigned long>(gMk64DistanceFogBinds3DS));
        std::fprintf(info, "Data cache clean path: %s\n", Mk64System3DSDataCacheMode());
        const auto& native = mk64_3ds::gNativeGeometry3DS.counters;
        const auto& reuse = mk64_3ds::gFast3DReuseStats;
        std::fprintf(info, "Native geometry lifetime loads / CPU XY materializations / eligible triangles / state switches: %llu / %llu / %llu / %llu\n",
                     static_cast<unsigned long long>(native.loaded),
                     static_cast<unsigned long long>(native.materialized),
                     static_cast<unsigned long long>(native.nativeTriangles),
                     static_cast<unsigned long long>(native.stateSwitches));
        std::fprintf(info, "Native geometry fallback disabled / mixed matrix / near eye / ineligible: %llu / %llu / %llu / %llu\n",
                     static_cast<unsigned long long>(native.fallbackDisabled),
                     static_cast<unsigned long long>(native.fallbackMixedMatrix),
                     static_cast<unsigned long long>(native.fallbackNearEye),
                     static_cast<unsigned long long>(native.fallbackIneligible));
        std::fprintf(info, "Native lighting lifetime loads / CPU materializations / eligible triangles: %llu / %llu / %llu\n",
                     static_cast<unsigned long long>(gMk64NativeLightLoaded3DS),
                     static_cast<unsigned long long>(gMk64NativeLightMaterialized3DS),
                     static_cast<unsigned long long>(gMk64NativeLightTriangles3DS));
        std::fprintf(info, "Exact reuse matrix hits / misses / depth block hits / misses: %lu / %lu / %lu / %lu\n",
                     static_cast<unsigned long>(reuse.matrixHits), static_cast<unsigned long>(reuse.matrixMisses),
                     static_cast<unsigned long>(reuse.positionHits), static_cast<unsigned long>(reuse.positionMisses));
        std::fprintf(info, "Audio missing envelopes silenced: %u\n",
                     __atomic_load_n(&gMk64AudioMissingEnvelope3DS, __ATOMIC_RELAXED));
        std::fprintf(info, "Captured Citro2D allocation bytes: %lu bytes\n",
                     static_cast<unsigned long>(
                         Mk64System3DSCapturedLinearAllocationSize()));
        std::fprintf(info, "Kernel / FIRM / system core: 0x%08lX / 0x%08lX / 0x%08lX\n",
                     static_cast<unsigned long>(osGetKernelVersion()),
                     static_cast<unsigned long>(osGetFirmVersion()),
                     static_cast<unsigned long>(osGetSystemCoreVersion()));
        std::fprintf(info, "Application memory free / size: %lu / %lu\n",
                     static_cast<unsigned long>(osGetMemRegionFree(MEMREGION_APPLICATION)),
                     static_cast<unsigned long>(osGetMemRegionSize(MEMREGION_APPLICATION)));
        std::fprintf(info, "System / base / linear free: %lu / %lu / %lu\n",
                     static_cast<unsigned long>(osGetMemRegionFree(MEMREGION_SYSTEM)),
                     static_cast<unsigned long>(osGetMemRegionFree(MEMREGION_BASE)),
                     static_cast<unsigned long>(linearSpaceFree()));
        std::fprintf(info, "Heap arena / used / free / keepcost: %d / %d / %d / %d\n",
                     heap.arena, heap.uordblks, heap.fordblks, heap.keepcost);
        std::fprintf(info, "Diagnostic thread SP / priority: 0x%08lX / %ld\n",
                     static_cast<unsigned long>(workerSp), static_cast<long>(priority));
        std::fprintf(info, "Game arena base / capacity: 0x%08lX / %lu\n",
                     static_cast<unsigned long>(sArenaBase.load(std::memory_order_relaxed)),
                     static_cast<unsigned long>(sArenaCapacity.load(std::memory_order_relaxed)));
        std::fprintf(info, "Display list base / bytes: 0x%08lX / %lu\n",
                     static_cast<unsigned long>(sDisplayListBase.load(std::memory_order_relaxed)),
                     static_cast<unsigned long>(sDisplayListSize.load(std::memory_order_relaxed)));
        std::fprintf(info, "Watchdog command / words / count: 0x%08lX / 0x%08lX 0x%08lX / %lu\n",
                     static_cast<unsigned long>(sWatchdogCommand.load(std::memory_order_relaxed)),
                     static_cast<unsigned long>(sWatchdogWord0.load(std::memory_order_relaxed)),
                     static_cast<unsigned long>(sWatchdogWord1.load(std::memory_order_relaxed)),
                     static_cast<unsigned long>(sWatchdogCount.load(std::memory_order_relaxed)));
        WriteMemoryMap(info);
        std::fclose(info);
    }

    if (!mk64_3ds::PerformanceWrite(directory)) LogLine("dump warning: ", "performance trace write failed");
    if (!mk64_3ds::InterpolationWriteDiagnostic(directory))
        LogLine("dump warning: ", "interpolation diagnostic write failed");
    ShowDumpProgress("METADATA", 1, 1);

    const uintptr_t arena = sArenaBase.load(std::memory_order_acquire);
    const size_t arenaCapacity = std::min(sArenaCapacity.load(std::memory_order_relaxed), kMaxArenaDump);
    if (arena != 0 && arenaCapacity != 0) {
        std::snprintf(path, sizeof(path), "%s/game-arena.bin", directory);
        sDumpProgressLabel = "GAME RAM";
        sDumpProgressDone = 0; sDumpProgressTotal = arenaCapacity;
        ShowDumpProgress(sDumpProgressLabel, 0, arenaCapacity);
        if (!WriteBlob(path, reinterpret_cast<const void*>(arena), arenaCapacity))
            LogLine("dump warning: ", "game arena write failed");
        sDumpProgressLabel = nullptr;
    }

    const uintptr_t displayList = sDisplayListBase.load(std::memory_order_acquire);
    const size_t displayListSize = std::min(sDisplayListSize.load(std::memory_order_relaxed), kMaxDisplayListDump);
    if (displayList != 0 && displayListSize != 0) {
        std::snprintf(path, sizeof(path), "%s/display-list.bin", directory);
        WriteBlob(path, reinterpret_cast<const void*>(displayList), displayListSize);
    }

    if (fullMemory) WriteProcessMemory(directory);
    CopyRuntimeLog(directory);
    LogLine("dump written: ", directory, true);
    ShowDumpSavedOverlay();
}

void DiagnosticThread(void*) {
    bool selectWasHeld = false;
    uint64_t lastRuntimeFlush = osGetTime();
    while (sRunning.load(std::memory_order_acquire)) {
        if (sAptSuspended.load(std::memory_order_acquire)) {
            selectWasHeld = false;
            svcSleepThread(16000000LL);
            continue;
        }
        hidScanInput();
        const uint32_t keys = hidKeysHeld();
        const uint32_t pressedKeys = hidKeysDown();
        const uint32_t releasedKeys = hidKeysUp();
        const uint32_t gameKeys = keys & ~static_cast<uint32_t>(KEY_SELECT);
        const uint32_t gameKeysDown = pressedKeys & ~static_cast<uint32_t>(KEY_SELECT);
        const uint32_t gameKeysUp = releasedKeys & ~static_cast<uint32_t>(KEY_SELECT);
        circlePosition circle = {};
        circlePosition cstick = {};
        hidCircleRead(&circle);
        if (sIsNew3DS) hidCstickRead(&cstick);
        if ((keys & KEY_TOUCH) != 0 || (pressedKeys & KEY_TOUCH) != 0) {
            touchPosition touch = {};
            hidTouchRead(&touch);
            sTouchX.store(touch.px, std::memory_order_relaxed);
            sTouchY.store(touch.py, std::memory_order_relaxed);
        }
        sCircleX.store(circle.dx, std::memory_order_relaxed);
        sCircleY.store(circle.dy, std::memory_order_relaxed);
        sCstickX.store(cstick.dx, std::memory_order_relaxed);
        sCstickY.store(cstick.dy, std::memory_order_relaxed);
        sTouchHeld.store((keys & KEY_TOUCH) != 0, std::memory_order_relaxed);
        if (gameKeysDown != 0) {
            sKeysDownLatched.fetch_or(gameKeysDown, std::memory_order_release);
        }
        if (gameKeysUp != 0) {
            sKeysUpLatched.fetch_or(gameKeysUp, std::memory_order_release);
        }
        sKeysHeld.store(gameKeys, std::memory_order_release);
        sInputReady.store(true, std::memory_order_release);

        const bool select = (keys & KEY_SELECT) != 0;
        const bool selectPressed = select && !selectWasHeld;
        if (selectPressed) {
            unsigned expected = kDumpRequestNone;
            const unsigned request = (keys & KEY_L) != 0 ? kDumpRequestFullMemory : kDumpRequestSelect;
            if (sDumpRequest.compare_exchange_strong(expected, request, std::memory_order_acq_rel)) {
                LogLine("dump requested: ", DumpTriggerName(request));
            }
        }
        selectWasHeld = select;
        const uint64_t now = osGetTime();
        if (now - lastRuntimeFlush >= kRuntimeFlushIntervalMilliseconds) {
            LogRuntimeHeartbeat();
            lastRuntimeFlush = now;
        }
        svcSleepThread(16000000LL);
    }
}

bool ReadInputSnapshot(Mk64DiagnosticsInput3DS* input, bool consumeEdges) {
    if (input == nullptr || !sInputReady.load(std::memory_order_acquire)) return false;
    input->heldMask = sKeysHeld.load(std::memory_order_acquire);
    input->downMask = consumeEdges
                          ? sKeysDownLatched.exchange(0, std::memory_order_acq_rel)
                          : sKeysDownLatched.load(std::memory_order_acquire);
    input->upMask = consumeEdges
                        ? sKeysUpLatched.exchange(0, std::memory_order_acq_rel)
                        : sKeysUpLatched.load(std::memory_order_acquire);
    input->circleX = static_cast<int16_t>(sCircleX.load(std::memory_order_relaxed));
    input->circleY = static_cast<int16_t>(sCircleY.load(std::memory_order_relaxed));
    input->cstickX = static_cast<int16_t>(sCstickX.load(std::memory_order_relaxed));
    input->cstickY = static_cast<int16_t>(sCstickY.load(std::memory_order_relaxed));
    input->touchX = static_cast<uint16_t>(sTouchX.load(std::memory_order_relaxed));
    input->touchY = static_cast<uint16_t>(sTouchY.load(std::memory_order_relaxed));
    input->touchHeld = sTouchHeld.load(std::memory_order_relaxed);
    return true;
}

} // namespace

extern "C" bool Mk64Diagnostics3DSStart() {
    if (sRunning.load(std::memory_order_acquire)) return true;
    LightLock_Init(&sTextLock);
    LightLock_Init(&sLogLock);
    sPendingLog.Clear();
    sBufferRuntimeLog = false;
    sInputReady.store(false, std::memory_order_relaxed);
    sKeysHeld.store(0, std::memory_order_relaxed);
    sKeysDownLatched.store(0, std::memory_order_relaxed);
    sKeysUpLatched.store(0, std::memory_order_relaxed);
    sTouchX.store(0, std::memory_order_relaxed);
    sTouchY.store(0, std::memory_order_relaxed);
    sTouchHeld.store(false, std::memory_order_relaxed);
    sLogLineCount.store(0, std::memory_order_relaxed);
    sLogFlushCount.store(0, std::memory_order_relaxed);
    sResourceUpdateCount.store(0, std::memory_order_relaxed);
    sCoursePrefetchDependencies.store(0, std::memory_order_relaxed);
    sCoursePrefetchLoaded.store(0, std::memory_order_relaxed);
    sCoursePrefetchBytes.store(0, std::memory_order_relaxed);
    sCoursePrefetchBudgetSkips.store(0, std::memory_order_relaxed);
    sCoursePrefetchUnavailable.store(0, std::memory_order_relaxed);
    sKartPrefetchAttempted.store(0, std::memory_order_relaxed);
    sKartPrefetchLoaded.store(0, std::memory_order_relaxed);
    sKartPrefetchBytes.store(0, std::memory_order_relaxed);
    sKartPrefetchDuplicates.store(0, std::memory_order_relaxed);
    sKartPrefetchUnavailable.store(0, std::memory_order_relaxed);
    sPrefetchUpdateSerial.store(0, std::memory_order_relaxed);
    sPrefetchLoggedSerial.store(0, std::memory_order_relaxed);
    sEmergencyWriteStarted.clear(std::memory_order_relaxed);
    EnsureDirectory(kGameDirectory);
    EnsureDirectory(kDumpDirectory);
    std::remove(kFatalLog);
    sStartTime = osGetTime();
    sSystemModelKnown = false;
    sSystemModel = 0xff;
    if (R_SUCCEEDED(cfguInit())) {
        sSystemModelKnown = R_SUCCEEDED(CFGU_GetSystemModel(&sSystemModel));
        cfguExit();
    }

    sIsNew3DS = false;
    if (R_FAILED(APT_CheckNew3DS(&sIsNew3DS))) {
        // If APT cannot answer, a successful CFGU model query still lets us
        // distinguish the New family. Otherwise unknown hardware remains on
        // the conservative Old/400 px path.
        sIsNew3DS = sSystemModelKnown &&
                    (sSystemModel == CFG_MODEL_N3DS ||
                     sSystemModel == CFG_MODEL_N3DSXL ||
                     sSystemModel == CFG_MODEL_N2DSXL);
    }
    sLog = std::fopen(kRuntimeLog, "wb");
    if (sLog != nullptr) {
        setvbuf(sLog, sLogBuffer, _IOFBF, sizeof(sLogBuffer));
    }
    sRunning.store(true, std::memory_order_release);
    s32 priority = 0x30;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    priority = std::min<s32>(priority + 1, 0x3f);
    sThread = threadCreate(DiagnosticThread, nullptr, 96u * 1024u, priority, -2, false);
    if (sThread == nullptr) {
        sRunning.store(false, std::memory_order_release);
        LogLine("diagnostic thread: ", "creation failed", true);
        if (sLog != nullptr) {
            std::fclose(sLog);
            sLog = nullptr;
        }
        return false;
    }
    LogLine("build: ", MK64_3DS_VERSION, true);
    Mk64Diagnostics3DSCheckpoint("diagnostics-started");
    return true;
}

extern "C" void Mk64Diagnostics3DSBufferRuntimeLog() {
    LightLock_Lock(&sLogLock);
    FlushLogLocked();
    sBufferRuntimeLog = true;
    LightLock_Unlock(&sLogLock);
}

extern "C" void Mk64Diagnostics3DSStop() {
    sRunning.store(false, std::memory_order_release);
    if (sThread != nullptr) {
        threadJoin(sThread, U64_MAX);
        threadFree(sThread);
        sThread = nullptr;
    }
    LightLock_Lock(&sLogLock);
    if (sLog != nullptr) {
        FlushLogLocked();
        std::fclose(sLog);
        sLog = nullptr;
    }
    LightLock_Unlock(&sLogLock);
}

extern "C" void Mk64Diagnostics3DSAbortForProcessExit() {
    sRunning.store(false, std::memory_order_release);
    if (sThread != nullptr) {
        // userAppExit runs after newlib's stdio exit handler but before
        // libctru unmaps HID. Join the poller without touching its FILE: exit
        // may already have closed it, while process termination reclaims it
        // on immediate _Exit paths.
        threadJoin(sThread, U64_MAX);
        threadFree(sThread);
        sThread = nullptr;
    }
    sLog = nullptr;
}

extern "C" void Mk64Diagnostics3DSEmergency(const char* reason) {
    if (sEmergencyWriteStarted.test_and_set(std::memory_order_acq_rel)) {
        return;
    }
    const int fd = open(kFatalLog, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        return;
    }
    WriteAll(fd, "fatal: ");
    WriteAll(fd, reason == nullptr ? "unknown" : reason);
    WriteAll(fd, "\n");
    if (LightLock_TryLock(&sTextLock) == 0) {
        WriteAll(fd, "stage: ");
        WriteAll(fd, sStage);
        WriteAll(fd, "\nresource: ");
        WriteAll(fd, sLastResource);
        WriteAll(fd, "\n");
        LightLock_Unlock(&sTextLock);
    } else {
        WriteAll(fd, "stage: unavailable (diagnostic lock held)\n");
    }
    fsync(fd);
    close(fd);
}

extern "C" bool Mk64Diagnostics3DSOwnsHid() {
    return sRunning.load(std::memory_order_acquire);
}

extern "C" void Mk64Diagnostics3DSSetAptSuspended(bool suspended) {
    sAptSuspended.store(suspended, std::memory_order_release);
}

extern "C" bool Mk64Diagnostics3DSIsPaused() {
    return sDumpPaused.load(std::memory_order_acquire);
}

extern "C" bool Mk64Diagnostics3DSServiceDumpIfRequested() {
    const unsigned request = sDumpRequest.exchange(kDumpRequestNone, std::memory_order_acq_rel);
    if (request == kDumpRequestNone) return false;

    const char* trigger = DumpTriggerName(request);
    sDumpPaused.store(true, std::memory_order_release);
    Mk64Diagnostics3DSSetStage("diagnostic-dump-paused");
    LogLine("dump trigger: ", trigger);
    if (Mk64GameAudio3DSSetPaused != nullptr) Mk64GameAudio3DSSetPaused(true);
    if (request == kDumpRequestClean) {
        Mk64BottomUI3DSShowProgress("CLEAN DUMPS", "DELETING FILES", 0);
        std::uint32_t next;
        bool ok = mk64_3ds::NextDumpNumber(kDumpDirectory, "sdmc:/3ds/MK64/dump-sequence.txt", &next) &&
            mk64_3ds::SaveDumpNumber("sdmc:/3ds/MK64/dump-sequence.txt", next);
        if (ok) {
            LightLock_Lock(&sLogLock);
            sPendingLog.Clear();
            if (sLog != nullptr) { std::fclose(sLog); sLog = nullptr; }
            ok = mk64_3ds::RemoveDumpContents(kDumpDirectory);
            sLog = std::fopen(kRuntimeLog, "wb");
            if (sLog != nullptr) setvbuf(sLog, sLogBuffer, _IOFBF, sizeof(sLogBuffer));
            else ok = false;
            LightLock_Unlock(&sLogLock);
        }
        Mk64BottomUI3DSShowProgress("CLEAN DUMPS", ok ? "DUMPS DELETED" : "CLEAN FAILED", ok ? 100 : 0);
        for (unsigned i = 0; i < 45; ++i) gspWaitForVBlank();
    } else {
        WriteQuickDump(trigger, request == kDumpRequestFullMemory);
    }
    if (Mk64GameAudio3DSSetPaused != nullptr) Mk64GameAudio3DSSetPaused(false);
    sDumpPaused.store(false, std::memory_order_release);
    return true;
}

extern "C" bool Mk64Diagnostics3DSReadInput(Mk64DiagnosticsInput3DS* input) {
    return ReadInputSnapshot(input, false);
}

extern "C" bool Mk64Diagnostics3DSConsumeInput(Mk64DiagnosticsInput3DS* input) {
    return ReadInputSnapshot(input, true);
}

extern "C" bool Mk64Diagnostics3DSRequestCleanDumps() {
    unsigned expected = kDumpRequestNone;
    return sDumpRequest.compare_exchange_strong(expected, kDumpRequestClean, std::memory_order_acq_rel);
}

extern "C" bool Mk64Diagnostics3DSRequestDump() {
    unsigned expected = kDumpRequestNone;
    if (!sDumpRequest.compare_exchange_strong(expected, kDumpRequestBottomUi,
                                               std::memory_order_acq_rel)) {
        return false;
    }
    LogLine("dump requested: ", DumpTriggerName(kDumpRequestBottomUi));
    return true;
}

extern "C" bool Mk64Diagnostics3DSIsNewModel() {
    return sIsNew3DS;
}

extern "C" bool Mk64Diagnostics3DSSupportsWideMode() {
    // libctru documents gfxSetWide as unsupported only on the Old 2DS. Keep
    // an unknown model at 400 px instead of risking an invalid display mode.
    return sSystemModelKnown && sSystemModel != CFG_MODEL_2DS;
}

extern "C" const char* Mk64Diagnostics3DSGetSystemModelName() {
    if (!sSystemModelKnown) return "UNKNOWN 3DS";
    switch (sSystemModel) {
        case CFG_MODEL_3DS: return "NINTENDO 3DS";
        case CFG_MODEL_3DSXL: return "NINTENDO 3DS XL";
        case CFG_MODEL_N3DS: return "NEW NINTENDO 3DS";
        case CFG_MODEL_2DS: return "NINTENDO 2DS";
        case CFG_MODEL_N3DSXL: return "NEW NINTENDO 3DS XL";
        case CFG_MODEL_N2DSXL: return "NEW NINTENDO 2DS XL";
        default: return "UNKNOWN 3DS";
    }
}

extern "C" void Mk64Diagnostics3DSCheckpoint(const char* stage) {
    Mk64Diagnostics3DSSetStage(stage);
    LogPrefetchSnapshotIfChanged();
    LogLine("stage: ", stage, true);
}

extern "C" void Mk64Diagnostics3DSSetStage(const char* stage) {
    LightLock_Lock(&sTextLock);
    CopyText(sStage, sizeof(sStage), stage);
    LightLock_Unlock(&sTextLock);
}

extern "C" void Mk64Diagnostics3DSFailure(const char* stage, const char* reason) {
    Mk64Diagnostics3DSSetStage(stage);
    LogLine("failure: ", reason, true);
}

extern "C" void Mk64Diagnostics3DSSetResource(const char* path, size_t loadedCount) {
    const size_t previousCount = sLoadedResources.exchange(loadedCount, std::memory_order_relaxed);
    LightLock_Lock(&sTextLock);
    CopyText(sLastResource, sizeof(sLastResource), path);
    LightLock_Unlock(&sTextLock);
    if (loadedCount != previousCount) {
        sResourceUpdateCount.fetch_add(1, std::memory_order_relaxed);
    }
}

extern "C" void Mk64Diagnostics3DSSetArchiveEntryCount(size_t entryCount) {
    sArchiveEntries.store(entryCount, std::memory_order_relaxed);
    char value[32] = {};
    std::snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(entryCount));
    LogLine("archive entries: ", value);
}

extern "C" void Mk64Diagnostics3DSCoursePrefetch(size_t dependencies, size_t loadedEntries,
                                                  size_t chargedBytes, size_t budgetSkips,
                                                  size_t unavailableEntries) {
    sCoursePrefetchDependencies.store(static_cast<uint32_t>(dependencies), std::memory_order_relaxed);
    sCoursePrefetchLoaded.store(static_cast<uint32_t>(loadedEntries), std::memory_order_relaxed);
    sCoursePrefetchBytes.store(static_cast<uint32_t>(chargedBytes), std::memory_order_relaxed);
    sCoursePrefetchBudgetSkips.store(static_cast<uint32_t>(budgetSkips), std::memory_order_relaxed);
    sCoursePrefetchUnavailable.store(static_cast<uint32_t>(unavailableEntries),
                                     std::memory_order_relaxed);
    sPrefetchUpdateSerial.fetch_add(1, std::memory_order_release);
}

extern "C" void Mk64Diagnostics3DSKartPrefetch(size_t attemptedEntries, size_t loadedEntries,
                                                size_t loadedBytes, size_t duplicateEntries,
                                                size_t unavailableEntries) {
    sKartPrefetchAttempted.store(static_cast<uint32_t>(attemptedEntries), std::memory_order_relaxed);
    sKartPrefetchLoaded.store(static_cast<uint32_t>(loadedEntries), std::memory_order_relaxed);
    sKartPrefetchBytes.store(static_cast<uint32_t>(loadedBytes), std::memory_order_relaxed);
    sKartPrefetchDuplicates.store(static_cast<uint32_t>(duplicateEntries), std::memory_order_relaxed);
    sKartPrefetchUnavailable.store(static_cast<uint32_t>(unavailableEntries),
                                   std::memory_order_relaxed);
    sPrefetchUpdateSerial.fetch_add(1, std::memory_order_release);
}

extern "C" void Mk64Diagnostics3DSSetGameArena(const void* base, size_t capacity) {
    sArenaCapacity.store(capacity, std::memory_order_relaxed);
    sArenaBase.store(reinterpret_cast<uintptr_t>(base), std::memory_order_release);
}

extern "C" void Mk64Diagnostics3DSSetDisplayList(const void* base, size_t size) {
    sDisplayListSize.store(size, std::memory_order_relaxed);
    sDisplayListBase.store(reinterpret_cast<uintptr_t>(base), std::memory_order_release);
}

extern "C" void Mk64Diagnostics3DSSetFrame(uint64_t frame, unsigned presentation) {
    sFrame.store(static_cast<uint32_t>(frame), std::memory_order_relaxed);
    sPresentation.store(presentation, std::memory_order_relaxed);
}

extern "C" void Mk64Diagnostics3DSAudio(uint32_t synthesisBlock, uint32_t bufferedFrames, uint32_t peak,
                                         uint32_t nonzeroSamples, uint32_t queuedBuffers,
                                         uint32_t droppedBuffers) {
    char value[128] = {};
    std::snprintf(value, sizeof(value), "block=%lu buffered=%lu peak=%lu nonzero=%lu queued=%lu dropped=%lu",
                  static_cast<unsigned long>(synthesisBlock), static_cast<unsigned long>(bufferedFrames),
                  static_cast<unsigned long>(peak), static_cast<unsigned long>(nonzeroSamples),
                  static_cast<unsigned long>(queuedBuffers), static_cast<unsigned long>(droppedBuffers));
    LogLine("audio: ", value);
}

extern "C" void Mk64Diagnostics3DSAudioPump(uint32_t pumpCalls, uint32_t synthesisBlocks,
                                              uint32_t bufferedBefore, uint32_t bufferedAfter,
                                              uint32_t blocksThisPump, uint32_t multiBlockPumps,
                                              uint32_t queueFailures,
                                              uint32_t observedEmptyTransitions) {
    char value[224] = {};
    std::snprintf(value, sizeof(value),
                  "calls=%lu blocks=%lu before=%lu after=%lu refill=%lu multi=%lu "
                  "queue_fail=%lu observed_empty=%lu",
                  static_cast<unsigned long>(pumpCalls),
                  static_cast<unsigned long>(synthesisBlocks),
                  static_cast<unsigned long>(bufferedBefore),
                  static_cast<unsigned long>(bufferedAfter),
                  static_cast<unsigned long>(blocksThisPump),
                  static_cast<unsigned long>(multiBlockPumps),
                  static_cast<unsigned long>(queueFailures),
                  static_cast<unsigned long>(observedEmptyTransitions));
    LogLine("audio-pump: ", value);
}

extern "C" void Mk64Diagnostics3DSAudioState(uint32_t resetStatus, uint32_t resetPreset,
                                             uint32_t sequenceCount, uint32_t activePlayers,
                                             uint32_t activeNotes, uint32_t audioErrors) {
    char value[160] = {};
    std::snprintf(value, sizeof(value),
                  "reset=%lu preset=%lu seqs=%lu activePlayers=%lu activeNotes=%lu errors=0x%08lX",
                  static_cast<unsigned long>(resetStatus), static_cast<unsigned long>(resetPreset),
                  static_cast<unsigned long>(sequenceCount), static_cast<unsigned long>(activePlayers),
                  static_cast<unsigned long>(activeNotes), static_cast<unsigned long>(audioErrors));
    LogLine("audio-state: ", value);
}

extern "C" void Mk64Diagnostics3DSPerformance(uint32_t frame, uint32_t fps2Tenths,
                                                uint32_t fps10Tenths, uint32_t drawCalls,
                                                uint32_t triangles, uint32_t textureUploads,
                                                uint32_t textureKilobytes, uint32_t vertexKilobytes,
                                                size_t loadedResources,
                                                uint32_t processingHundredths,
                                                uint32_t drawingHundredths,
                                                uint32_t commandPermille,
                                                uint32_t linearHeapFlushFrames) {
    char value[288] = {};
    std::snprintf(value, sizeof(value),
                  "frame=%lu fps2=%lu.%lu fps10=%lu.%lu draws=%lu tris=%lu "
                  "tex_up=%lu tex_kib=%lu vtx_kib=%lu resources=%lu "
                  "c3d_process=%lu.%02lu c3d_draw=%lu.%02lu cmd=%lu.%01lu%% linear_full=%lu",
                  static_cast<unsigned long>(frame),
                  static_cast<unsigned long>(fps2Tenths / 10U),
                  static_cast<unsigned long>(fps2Tenths % 10U),
                  static_cast<unsigned long>(fps10Tenths / 10U),
                  static_cast<unsigned long>(fps10Tenths % 10U),
                  static_cast<unsigned long>(drawCalls),
                  static_cast<unsigned long>(triangles),
                  static_cast<unsigned long>(textureUploads),
                  static_cast<unsigned long>(textureKilobytes),
                  static_cast<unsigned long>(vertexKilobytes),
                  static_cast<unsigned long>(loadedResources),
                  static_cast<unsigned long>(processingHundredths / 100U),
                  static_cast<unsigned long>(processingHundredths % 100U),
                  static_cast<unsigned long>(drawingHundredths / 100U),
                  static_cast<unsigned long>(drawingHundredths % 100U),
                  static_cast<unsigned long>(commandPermille / 10U),
                  static_cast<unsigned long>(commandPermille % 10U),
                  static_cast<unsigned long>(linearHeapFlushFrames));
    LogLine("performance: ", value);
}

extern "C" void Mk64Diagnostics3DSMemory(const char* label, size_t loadedResources, size_t textureSlots,
                                         size_t initializedTextures, size_t textureBytes,
                                         size_t shaderPrograms, size_t clipScratchBytes) {
    const struct mallinfo heap = mallinfo();
    char value[320] = {};
    std::snprintf(value, sizeof(value),
                  "%s resources=%lu texSlots=%lu texLive=%lu texBytes=%lu shaders=%lu clipScratch=%lu "
                  "appFree=%lu heapFree=%d heapUsed=%d linearFree=%lu vramFree=%lu",
                  label == nullptr ? "snapshot" : label,
                  static_cast<unsigned long>(loadedResources),
                  static_cast<unsigned long>(textureSlots),
                  static_cast<unsigned long>(initializedTextures),
                  static_cast<unsigned long>(textureBytes),
                  static_cast<unsigned long>(shaderPrograms),
                  static_cast<unsigned long>(clipScratchBytes),
                  static_cast<unsigned long>(osGetMemRegionFree(MEMREGION_APPLICATION)),
                  heap.fordblks, heap.uordblks,
                  static_cast<unsigned long>(linearSpaceFree()),
                  static_cast<unsigned long>(vramSpaceFree()));
    LogLine("memory: ", value);
}

extern "C" void Mk64Diagnostics3DSGfxWatchdog(const void* command, uint32_t word0, uint32_t word1,
                                                size_t commandCount) {
    sWatchdogCommand.store(reinterpret_cast<uintptr_t>(command), std::memory_order_relaxed);
    sWatchdogWord0.store(word0, std::memory_order_relaxed);
    sWatchdogWord1.store(word1, std::memory_order_relaxed);
    sWatchdogCount.store(commandCount, std::memory_order_relaxed);
    Mk64Diagnostics3DSSetStage("renderer-command-budget-exceeded");
    LogLine("renderer watchdog: ", "display list command budget exceeded", true);
}
