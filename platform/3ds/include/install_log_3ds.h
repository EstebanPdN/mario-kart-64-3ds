#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Kept as a C symbol so the patched Torch archive writer can report its own
// storage failures without depending on the 3DS application's C++ internals.
void Mk64InstallLogWrite(const char* message);

typedef void (*Mk64InstallLogCallback)(const char* message);
void Mk64InstallLogSetCallback(Mk64InstallLogCallback callback);

#ifdef __cplusplus
}

void Mk64InstallLogBegin();
void Mk64InstallLogWritef(const char* format, ...);
void Mk64InstallLogClose();
#endif
