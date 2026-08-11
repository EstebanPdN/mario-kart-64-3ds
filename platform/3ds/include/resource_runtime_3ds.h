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
} Mk64TextureResource3DS;

/* Resolve texture bytes and metadata with one archive/cache lookup. */
bool Mk64Resource3DSGetTexture(const char* name, Mk64TextureResource3DS* outTexture);

#ifdef __cplusplus
}
#endif
