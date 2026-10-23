#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Main thread, after RomFS mount and before archive residency admission.
 * textureMode is Original=0 / Improved=1 / Menu=2. A false result leaves the
 * previous catalog intact; artwork is optional and source fallback remains. */
bool Mk64Improved3DSConfigure(int textureMode);
/* Loading boundary only. budget is *additional* heap allowance after keeping
 * the ordinary 8 MiB reserve. Returns true only for validated kart residency.
 * Failure can still prepare a bounded decoded page cache for source reads. */
bool Mk64Improved3DSPrepare(size_t budget, bool newModel);
bool Mk64Improved3DSReclaim(void);
size_t Mk64Improved3DSMetadataBytes(void);
size_t Mk64Improved3DSResidentBytes(void);
size_t Mk64Improved3DSPageBytes(void);
/* Explicit paused export only. 32 bounded RAM events, no steady file logging. */
bool Mk64Improved3DSWriteHistory(const char* directory);
/* Loading boundary only; warms the normal texture path and its bounded
 * owners. Exact names include recurrent track textures, kart/palette variants
 * and effects. Nontexture ZIP dependencies use O2rArchiveReader::CacheEntries. */
size_t Mk64Improved3DSPreload(const char* const* names, size_t count);
#ifdef __cplusplus
}
#endif
