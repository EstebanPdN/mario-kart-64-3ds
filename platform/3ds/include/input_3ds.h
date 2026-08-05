#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum Mk64N64Button {
    MK64_N64_CRIGHT = 0x0001,
    MK64_N64_CLEFT = 0x0002,
    MK64_N64_CDOWN = 0x0004,
    MK64_N64_CUP = 0x0008,
    MK64_N64_R = 0x0010,
    MK64_N64_L = 0x0020,
    MK64_N64_DRIGHT = 0x0100,
    MK64_N64_DLEFT = 0x0200,
    MK64_N64_DDOWN = 0x0400,
    MK64_N64_DUP = 0x0800,
    MK64_N64_START = 0x1000,
    MK64_N64_Z = 0x2000,
    MK64_N64_B = 0x4000,
    MK64_N64_A = 0x8000,
};

typedef struct Mk64Pad3DS {
    uint16_t buttons;
    int8_t stickX;
    int8_t stickY;
    int8_t rightStickX;
    int8_t rightStickY;
} Mk64Pad3DS;

void Mk64Input3DSInit(void);
void Mk64Input3DSPoll(Mk64Pad3DS* pad);

#ifdef __cplusplus
}
#endif
