#pragma once

typedef int SDL_MessageBoxFlags;

#define SDL_MESSAGEBOX_ERROR 0x00000010
#define SDL_MESSAGEBOX_WARNING 0x00000020
#define SDL_MESSAGEBOX_INFORMATION 0x00000040

inline int SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags, const char*, const char*, void*) {
    return 0;
}
