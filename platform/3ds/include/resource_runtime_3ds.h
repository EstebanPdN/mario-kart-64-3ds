#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Mk64Resource3DSInit(const char* archivePath);
void Mk64Resource3DSShutdown(void);
size_t Mk64Resource3DSArchiveEntryCount(void);
size_t Mk64Resource3DSLoadedCount(void);

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

/* Resolve texture bytes and metadata with one archive/cache lookup. */
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

/* Exhaustive host-probe check for the compact CRC index. */
bool Mk64Resource3DSValidateCrcIndex(void);

/* Query serialized entry size without reading or caching its payload. */
bool Mk64Resource3DSGetArchiveEntrySizeByName(const char* name, size_t* byteCount);
bool Mk64Resource3DSGetArchiveEntrySizeByCrc(uint64_t crc, size_t* byteCount);

#ifdef __cplusplus
}
#endif
