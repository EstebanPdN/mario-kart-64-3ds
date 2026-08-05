#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Mk64GameAudio3DSInit(void);
void Mk64GameAudio3DSPump(void);
void Mk64GameAudio3DSShutdown(void);

#ifdef __cplusplus
}
#endif
