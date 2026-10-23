#pragma once
#include <stdint.h>
/* MK64 actor rules, kept outside the reusable native GPU scene. */
static inline uint16_t Mk64NativeRedShellColor(uint16_t pixel) {
    /* Preserve the N64 source's signed red component and arithmetic division.
       Read/write big-endian bytes explicitly on ARM; never swap partial fields
       before shifting them. Division is exact for this multiple-of-2048 input. */
    int32_t red = (int32_t)(pixel & 0xf800u);
    if (red & 0x8000) red -= 65536;
    return (uint16_t)((red / 32) | ((int32_t)(pixel & 0x07c0u) * 32) | (pixel & 0x003fu));
}
static inline void Mk64NativeBuildRedShellPalette(const void* input,void* output) {
    const uint8_t* source=(const uint8_t*)input;uint8_t* target=(uint8_t*)output;
    for(unsigned i=0;i<256;++i) {
        uint16_t word=(uint16_t)(((uint16_t)source[2*i]<<8)|source[2*i+1]);
        word=Mk64NativeRedShellColor(word);target[2*i]=(uint8_t)(word>>8);target[2*i+1]=(uint8_t)word;
    }
}
