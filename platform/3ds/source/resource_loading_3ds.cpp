#include "resource_loading_3ds.h"
#include "resource_runtime_3ds.h"
#include "improved_resources_3ds.h"
#include "heap_policy_3ds.hpp"
#include "native_preload_3ds.hpp"
#include "audio_runtime_3ds.h"
#include "game_runtime_3ds.h"
#include "settings_3ds.h"
#include "diagnostics_3ds.h"
#include <3ds.h>
#include <malloc.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <vector>

extern "C" const char* ResourceGetNameByCrc(uint64_t crc);
namespace {
size_t Allowance() {
    return mk64_3ds::ArchiveResidencyBudget(envGetHeapSize(), static_cast<size_t>(mallinfo().uordblks));
}
void LoadingProgress(unsigned) { Mk64GameAudio3DSServiceLoading(); }
struct LoadingAudio {
    LoadingAudio() { Mk64GameAudio3DSBeginLoading(); }
    ~LoadingAudio() { Mk64GameAudio3DSEndLoading(); }
};
}

extern "C" void Mk64Resource3DSPrepareCourse(const char* directory) {
    if (!directory) return;
    LoadingAudio audio;
    try {
        // Retry only at a course boundary, after old borrowers were released.
        // A failed optional cache keeps the ordinary validated streaming path.
        Mk64Improved3DSConfigure(Mk64Settings3DSGetUiTextures());
        if (!Mk64Resource3DSIsResident()) {
            // Residency replaces the compressed streaming cache. Release it
            // before measuring actual heap use; a clamped allowance cannot
            // safely be increased by an estimated cache allocation afterward.
            Mk64Resource3DSSetStreamingCacheBudget(0);
            // MakeResident restores the ordinary streaming limit on failure.
            Mk64Resource3DSMakeResident(Allowance(), LoadingProgress);
        }
        Mk64Improved3DSPrepare(Allowance(), Mk64Graphics3DSResolvedNewModel());
        char path[160];
        const int length = std::snprintf(path, sizeof(path), "romfs:/course-preloads/%s/resources.npr", directory);
        if (length < 0 || static_cast<size_t>(length) >= sizeof(path)) return;
        std::unique_ptr<FILE, int(*)(FILE*)> file(std::fopen(path, "rb"), std::fclose);
        if (!file || std::fseek(file.get(), 0, SEEK_END)) return;
        const long size = std::ftell(file.get());
        if (size < 8 || size > 8 + 8192 * 16 || Allowance() < static_cast<size_t>(size) * 3 + 65536 ||
            std::fseek(file.get(), 0, SEEK_SET)) return;
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        if (std::fread(bytes.data(), 1, bytes.size(), file.get()) != bytes.size()) return;
        file.reset();
        std::vector<mk64_3ds::NativeDependency> dependencies;
        if (!mk64_3ds::ValidateNativeDependencies(bytes.data(), bytes.size(), dependencies)) return;
        size_t compressed = 0, textures = 0;
        for (const auto& dependency : dependencies) {
            const char* name = ResourceGetNameByCrc(dependency.hash);
            if (!name) continue;
            if (!Mk64Resource3DSIsResident()) {
                // Decoded warming below changes heap use between entries.
                // CacheRange accepts a total cache ceiling, so include only
                // this cache's replaceable payload in the fresh allowance;
                // keep allocator/index overhead charged to the used heap.
                Mk64ResourceCacheStats3DS cache = {};
                Mk64Resource3DSGetCacheStats(&cache);
                const size_t existing = std::min(static_cast<size_t>(cache.streamPayloadBytes),
                                                mk64_3ds::kStreamingCacheBytes);
                const size_t streamBudget = existing + std::min(Allowance(),
                    mk64_3ds::kStreamingCacheBytes - existing);
                compressed += Mk64Resource3DSPrefetchNames(&name, 1, streamBudget);
            }
            // Leave transient decoder space beyond the permanent reserve.
            // Pixel owners use the existing bounded decoded caches.
            if (dependency.kind == 1 && Allowance() > static_cast<size_t>(dependency.bytes) + 512 * 1024)
                textures += Mk64Improved3DSPreload(&name, 1);
            Mk64GameAudio3DSServiceLoading();
        }
        char report[192];
        std::snprintf(report, sizeof(report), "course-preload %s dependencies=%lu compressed=%lu textures=%lu art-resident=%lu pages=%lu",
            directory, static_cast<unsigned long>(dependencies.size()), static_cast<unsigned long>(compressed),
            static_cast<unsigned long>(textures), static_cast<unsigned long>(Mk64Improved3DSResidentBytes()),
            static_cast<unsigned long>(Mk64Improved3DSPageBytes()));
        Mk64Diagnostics3DSCheckpoint(report);
    } catch (const std::bad_alloc&) {
        Mk64Diagnostics3DSCheckpoint("course-preload optional allocation unavailable");
    }
}
