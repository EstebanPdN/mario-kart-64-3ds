#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Copies the 128x96 centre-camera region from the most recently presented
// top-screen image into the six 64x32 RGBA5551 textures used by the Luigi
// Raceway and Wario Stadium jumbotrons.
void Mk64Jumbotron3DSUpdate(uint16_t* topLeft, uint16_t* topRight,
                           uint16_t* middleLeft, uint16_t* middleRight,
                           uint16_t* bottomLeft, uint16_t* bottomRight);

#ifdef __cplusplus
}
#endif
