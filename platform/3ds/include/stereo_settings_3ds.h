#pragma once

#include <stdbool.h>

// Top-screen stereo: the hardware 3D slider sets the strength, these the rest.
typedef struct Mk64Stereo3DS {
    int separation;         // 0..100: 3D depth at full slider
    int convergenceTenths;  // 1..200: manual convergence (a ceiling under auto)
    int hudDepth;           // 0..100: 50 is the screen plane
    bool autoConvergence;
    int comfortTarget;      // -20..30: auto convergence's pop-out budget, thousandths of width
} Mk64Stereo3DS;
