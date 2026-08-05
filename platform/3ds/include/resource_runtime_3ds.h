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

#ifdef __cplusplus
}
#endif
