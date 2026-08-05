#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Mk64Graphics3DSInit(void);
void Mk64Graphics3DSShutdown(void);
bool WindowIsRunning(void);

#ifdef __cplusplus
}
#endif
