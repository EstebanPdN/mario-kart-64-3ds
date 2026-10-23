#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Main-thread resource policy; independent probes leave replacements disabled. */
void Mk64Resource3DSSetImprovedTexturePolicy(bool (*enabled)(const char* name));

/* Loading-phase ZIP LRU cache; false means caching was unavailable, not a missing asset. */
bool Mk64Resource3DSPrefetchKart(const char* name, size_t* cachedBytes);
/* Loading boundary, exact dependencies (course lists, palettes, karts, effects).
 * Shares the streaming cache; budget includes its index, bounded to 2 MiB.
 * Optional failures preserve normal validated reads and all resource pixels. */
size_t Mk64Resource3DSPrefetchNames(const char* const* names, size_t count, size_t budget);
typedef struct Mk64ResourceCacheStats3DS {
    size_t streamEntries, streamPayloadBytes, streamAllocationBytes;
    uint64_t streamHits, streamMisses, streamByteEvictions;
    uint64_t retentionStaleRemoved, retentionByteEvictions, retentionCountEvictions;
} Mk64ResourceCacheStats3DS;
void Mk64Resource3DSGetCacheStats(Mk64ResourceCacheStats3DS* stats);
bool Mk64Resource3DSInit(const char* archivePath);
bool Mk64Resource3DSMakeResident(size_t budget, void (*progress)(unsigned));
bool Mk64Resource3DSDropResidency(void);
/* new_handler and resource-retry path: Improved -> unowned decoded -> native
 * -> O2R. Refuses reentrant cache mutation and reclaims one tier per call. */
bool Mk64Resource3DSReclaimPerformanceCache(void);
/* Exact last surrendered tier; use this checkpoint instead of inferring
 * archive release from IsResident() before an Improved-only reclamation. */
const char* Mk64Resource3DSLastReclaimedCache(void);
/* Loading phase only: changing the budget may evict unpinned archive bytes. */
void Mk64Resource3DSSetStreamingCacheBudget(size_t budget);
/* Paused dump only; 64 bounded in-RAM actual loads, with physical SD bytes. */
bool Mk64Resource3DSWriteLoadHistory(const char* directory);
bool Mk64Resource3DSIsResident(void);
bool Mk64Resource3DSResidencyMemoryLimited(void);
const char* Mk64Resource3DSResidencyStatus(void);
size_t Mk64Resource3DSResidentRequiredBytes(void);
uint64_t Mk64Resource3DSArchiveBytes(void);
uint64_t Mk64Resource3DSPhysicalReadCalls(void);
uint64_t Mk64Resource3DSPhysicalReadBytes(void);
size_t Mk64Resource3DSStreamingCachedBytes(void);
void Mk64Resource3DSShutdown(void);
size_t Mk64Resource3DSArchiveEntryCount(void);
size_t Mk64Resource3DSLoadedCount(void);
/* Immutable archive identity; zero while closed. Canonical names remain valid
 * only while this nonzero generation is unchanged. Main-thread use only. */
uint32_t Mk64Resource3DSGeneration(void);

/* Retain the selected stock course and evict decoded data from other courses. */
size_t Mk64Resource3DSSetActiveTrack(const char* assetDirectory);

/* Call only after the old scene and GPU references have been destroyed. */
size_t Mk64Resource3DSReleaseRaceResources(void);
typedef struct Mk64RaceResourceStats3DS {
    size_t entries;
    size_t bytes;
    size_t kartEntries;
    size_t rawEntries;
    size_t ownedEntries;
} Mk64RaceResourceStats3DS;
void Mk64Resource3DSGetRaceStats(Mk64RaceResourceStats3DS* stats);

typedef struct Mk64TextureResource3DS {
    const uint8_t* data;
    size_t size;
    uint16_t width;
    uint16_t height;
    uint32_t type;
    const char* canonicalName;
    uint64_t lifetimeToken;
} Mk64TextureResource3DS;

typedef enum Mk64ResourceLoadResult3DS {
    MK64_RESOURCE_LOAD_OK_3DS = 0,
    MK64_RESOURCE_LOAD_NOT_FOUND_3DS = 1,
    MK64_RESOURCE_LOAD_OUT_OF_MEMORY_3DS = 2,
} Mk64ResourceLoadResult3DS;

/* Borrow texture bytes until the next resource load/eviction. Do not retain
 * data across another lookup or an allocation that can reclaim caches. Use
 * AcquireTexture/ReleaseTexture (or TextureLease3DS) while decoding/uploading. */
bool Mk64Resource3DSGetTexture(const char* name, Mk64TextureResource3DS* outTexture);

/* Keep texture bytes resident until the matching lifetime token is released. */
Mk64ResourceLoadResult3DS Mk64Resource3DSAcquireTexture(
    const char* name, Mk64TextureResource3DS* outTexture);
void Mk64Resource3DSReleaseTexture(uint64_t lifetimeToken);

/* Renderer-facing lookups preserve out-of-memory as a distinct result. */
Mk64ResourceLoadResult3DS Mk64Resource3DSResolveDataByName(const char* name,
                                                           void** outData);
Mk64ResourceLoadResult3DS Mk64Resource3DSResolveDataByCrc(uint64_t crc,
                                                          void** outData);

/* Pin a decoded vertex/light/other resource through an explicit release. */
Mk64ResourceLoadResult3DS Mk64Resource3DSAcquireDataByCrc(
    uint64_t crc, void** outData, uint64_t* lifetimeToken);
void Mk64Resource3DSReleaseData(uint64_t lifetimeToken);

/* Resolve loaded procedure identity without reading serialized graphics words. */
bool Mk64Resource3DSDisplayListIdentity(const void* pointer, uint64_t* hash, size_t* entry);

/* Exhaustive host-probe check for the compact CRC index. */
bool Mk64Resource3DSValidateCrcIndex(void);

/* Query serialized entry size without reading or caching its payload. */
bool Mk64Resource3DSGetArchiveEntrySizeByName(const char* name, size_t* byteCount);
bool Mk64Resource3DSGetArchiveEntrySizeByCrc(uint64_t crc, size_t* byteCount);

#ifdef __cplusplus
}
#endif
