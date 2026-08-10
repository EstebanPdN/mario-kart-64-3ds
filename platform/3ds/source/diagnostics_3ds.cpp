#include "diagnostics_3ds.h"

#include <3ds.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <malloc.h>
#include <sys/stat.h>

namespace {

constexpr const char* kGameDirectory = "sdmc:/3ds/MK64";
constexpr const char* kDumpDirectory = "sdmc:/3ds/MK64/dump";
constexpr const char* kRuntimeLog = "sdmc:/3ds/MK64/dump/runtime.log";
constexpr size_t kMaxArenaDump = 16u * 1024u * 1024u;
constexpr size_t kMaxDisplayListDump = 2u * 1024u * 1024u;

std::atomic<bool> sRunning{ false };
std::atomic<bool> sInputReady{ false };
std::atomic<uint32_t> sKeysHeld{ 0 };
std::atomic<int> sCircleX{ 0 };
std::atomic<int> sCircleY{ 0 };
std::atomic<int> sCstickX{ 0 };
std::atomic<int> sCstickY{ 0 };
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

LightLock sTextLock;
char sStage[96] = "not-started";
char sLastResource[256] = "none";
Thread sThread = nullptr;
FILE* sLog = nullptr;
bool sIsNew3DS = false;
uint64_t sStartTime = 0;

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

void LogLine(const char* prefix, const char* value) {
    LightLock_Lock(&sTextLock);
    if (sLog != nullptr) {
        std::fprintf(sLog, "%llu %s%s\n",
                     static_cast<unsigned long long>(osGetTime() - sStartTime),
                     prefix == nullptr ? "" : prefix, value == nullptr ? "unknown" : value);
        std::fflush(sLog);
    }
    LightLock_Unlock(&sTextLock);
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

bool WriteBlob(const char* path, const void* data, size_t size) {
    if (path == nullptr || !IsReadableRange(reinterpret_cast<uintptr_t>(data), size)) return false;
    FILE* file = std::fopen(path, "wb");
    if (file == nullptr) return false;
    bool ok = std::fwrite(data, 1, size, file) == size;
    if (std::fclose(file) != 0) ok = false;
    if (!ok) {
        std::remove(path);
    }
    return ok;
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
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        if (attempt == 0) {
            std::snprintf(output, outputSize, "%s/dump-%s", kDumpDirectory, stamp);
        } else {
            std::snprintf(output, outputSize, "%s/dump-%s-%02u", kDumpDirectory, stamp, attempt);
        }
        if (mkdir(output, 0777) == 0) return true;
        if (errno != EEXIST) break;
    }
    output[0] = '\0';
    return false;
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
    LightLock_Lock(&sTextLock);
    if (sLog != nullptr) std::fflush(sLog);
    LightLock_Unlock(&sTextLock);

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

void WriteQuickDump() {
    char directory[192] = {};
    if (!CreateSessionDirectory(directory, sizeof(directory))) {
        LogLine("dump failed: ", "could not create session directory");
        return;
    }

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
        std::fprintf(info, "Trigger: L + R + A\n");
        std::fprintf(info, "Uptime ms: %llu\n",
                     static_cast<unsigned long long>(osGetTime() - sStartTime));
        std::fprintf(info, "Stage: %s\n", stage);
        std::fprintf(info, "Last resource: %s\n", resource);
        std::fprintf(info, "Archive entries / loaded resources: %lu / %lu\n",
                     static_cast<unsigned long>(sArchiveEntries.load(std::memory_order_relaxed)),
                     static_cast<unsigned long>(sLoadedResources.load(std::memory_order_relaxed)));
        std::fprintf(info, "Frame / presentation: %lu / %u\n",
                     static_cast<unsigned long>(sFrame.load(std::memory_order_relaxed)),
                     sPresentation.load(std::memory_order_relaxed));
        std::fprintf(info, "Keys held: 0x%08lX\n",
                     static_cast<unsigned long>(sKeysHeld.load(std::memory_order_relaxed)));
        std::fprintf(info, "Model: %s\n", sIsNew3DS ? "New 3DS" : "Old 3DS / unknown");
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

    const uintptr_t arena = sArenaBase.load(std::memory_order_acquire);
    const size_t arenaCapacity = std::min(sArenaCapacity.load(std::memory_order_relaxed), kMaxArenaDump);
    if (arena != 0 && arenaCapacity != 0) {
        std::snprintf(path, sizeof(path), "%s/game-arena.bin", directory);
        WriteBlob(path, reinterpret_cast<const void*>(arena), arenaCapacity);
    }

    const uintptr_t displayList = sDisplayListBase.load(std::memory_order_acquire);
    const size_t displayListSize = std::min(sDisplayListSize.load(std::memory_order_relaxed), kMaxDisplayListDump);
    if (displayList != 0 && displayListSize != 0) {
        std::snprintf(path, sizeof(path), "%s/display-list.bin", directory);
        WriteBlob(path, reinterpret_cast<const void*>(displayList), displayListSize);
    }

    CopyRuntimeLog(directory);
    LogLine("dump written: ", directory);
}

void DiagnosticThread(void*) {
    bool comboWasHeld = false;
    while (sRunning.load(std::memory_order_acquire)) {
        hidScanInput();
        const uint32_t keys = hidKeysHeld();
        circlePosition circle = {};
        circlePosition cstick = {};
        hidCircleRead(&circle);
        if (sIsNew3DS) hidCstickRead(&cstick);
        sCircleX.store(circle.dx, std::memory_order_relaxed);
        sCircleY.store(circle.dy, std::memory_order_relaxed);
        sCstickX.store(cstick.dx, std::memory_order_relaxed);
        sCstickY.store(cstick.dy, std::memory_order_relaxed);
        sKeysHeld.store(keys, std::memory_order_release);
        sInputReady.store(true, std::memory_order_release);

        const bool combo = (keys & (KEY_L | KEY_R | KEY_A)) == (KEY_L | KEY_R | KEY_A);
        if (combo && !comboWasHeld) WriteQuickDump();
        comboWasHeld = combo;
        svcSleepThread(16000000LL);
    }
}

} // namespace

extern "C" bool Mk64Diagnostics3DSStart() {
    if (sRunning.load(std::memory_order_acquire)) return true;
    LightLock_Init(&sTextLock);
    EnsureDirectory(kGameDirectory);
    EnsureDirectory(kDumpDirectory);
    sStartTime = osGetTime();
    APT_CheckNew3DS(&sIsNew3DS);
    sLog = std::fopen(kRuntimeLog, "wb");
    if (sLog != nullptr) setvbuf(sLog, nullptr, _IOLBF, 0);
    sRunning.store(true, std::memory_order_release);
    s32 priority = 0x30;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    priority = std::min<s32>(priority + 1, 0x3f);
    sThread = threadCreate(DiagnosticThread, nullptr, 96u * 1024u, priority, -2, false);
    if (sThread == nullptr) {
        sRunning.store(false, std::memory_order_release);
        LogLine("diagnostic thread: ", "creation failed");
        if (sLog != nullptr) {
            std::fclose(sLog);
            sLog = nullptr;
        }
        return false;
    }
    Mk64Diagnostics3DSCheckpoint("diagnostics-started");
    return true;
}

extern "C" void Mk64Diagnostics3DSStop() {
    sRunning.store(false, std::memory_order_release);
    if (sThread != nullptr) {
        threadJoin(sThread, U64_MAX);
        threadFree(sThread);
        sThread = nullptr;
    }
    LightLock_Lock(&sTextLock);
    if (sLog != nullptr) {
        std::fflush(sLog);
        std::fclose(sLog);
        sLog = nullptr;
    }
    LightLock_Unlock(&sTextLock);
}

extern "C" bool Mk64Diagnostics3DSOwnsHid() {
    return sRunning.load(std::memory_order_acquire);
}

extern "C" bool Mk64Diagnostics3DSReadInput(Mk64DiagnosticsInput3DS* input) {
    if (input == nullptr || !sInputReady.load(std::memory_order_acquire)) return false;
    input->heldMask = sKeysHeld.load(std::memory_order_acquire);
    input->circleX = static_cast<int16_t>(sCircleX.load(std::memory_order_relaxed));
    input->circleY = static_cast<int16_t>(sCircleY.load(std::memory_order_relaxed));
    input->cstickX = static_cast<int16_t>(sCstickX.load(std::memory_order_relaxed));
    input->cstickY = static_cast<int16_t>(sCstickY.load(std::memory_order_relaxed));
    return true;
}

extern "C" void Mk64Diagnostics3DSCheckpoint(const char* stage) {
    Mk64Diagnostics3DSSetStage(stage);
    LogLine("stage: ", stage);
}

extern "C" void Mk64Diagnostics3DSSetStage(const char* stage) {
    LightLock_Lock(&sTextLock);
    CopyText(sStage, sizeof(sStage), stage);
    LightLock_Unlock(&sTextLock);
}

extern "C" void Mk64Diagnostics3DSFailure(const char* stage, const char* reason) {
    Mk64Diagnostics3DSSetStage(stage);
    LogLine("failure: ", reason);
}

extern "C" void Mk64Diagnostics3DSSetResource(const char* path, size_t loadedCount) {
    const size_t previousCount = sLoadedResources.exchange(loadedCount, std::memory_order_relaxed);
    LightLock_Lock(&sTextLock);
    CopyText(sLastResource, sizeof(sLastResource), path);
    LightLock_Unlock(&sTextLock);
    if (loadedCount != previousCount && loadedCount <= 256) {
        LogLine("resource: ", path);
    }
}

extern "C" void Mk64Diagnostics3DSSetArchiveEntryCount(size_t entryCount) {
    sArchiveEntries.store(entryCount, std::memory_order_relaxed);
    char value[32] = {};
    std::snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(entryCount));
    LogLine("archive entries: ", value);
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

extern "C" void Mk64Diagnostics3DSAudio(uint32_t pump, uint32_t bufferedFrames, uint32_t peak,
                                         uint32_t nonzeroSamples, uint32_t queuedBuffers,
                                         uint32_t droppedBuffers) {
    char value[128] = {};
    std::snprintf(value, sizeof(value), "pump=%lu buffered=%lu peak=%lu nonzero=%lu queued=%lu dropped=%lu",
                  static_cast<unsigned long>(pump), static_cast<unsigned long>(bufferedFrames),
                  static_cast<unsigned long>(peak), static_cast<unsigned long>(nonzeroSamples),
                  static_cast<unsigned long>(queuedBuffers), static_cast<unsigned long>(droppedBuffers));
    LogLine("audio: ", value);
}

extern "C" void Mk64Diagnostics3DSGfxWatchdog(const void* command, uint32_t word0, uint32_t word1,
                                                size_t commandCount) {
    sWatchdogCommand.store(reinterpret_cast<uintptr_t>(command), std::memory_order_relaxed);
    sWatchdogWord0.store(word0, std::memory_order_relaxed);
    sWatchdogWord1.store(word1, std::memory_order_relaxed);
    sWatchdogCount.store(commandCount, std::memory_order_relaxed);
    Mk64Diagnostics3DSSetStage("renderer-command-budget-exceeded");
    LogLine("renderer watchdog: ", "display list command budget exceeded");
}
