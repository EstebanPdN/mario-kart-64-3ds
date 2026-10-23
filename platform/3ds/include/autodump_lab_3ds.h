#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool Mk64Lab3DSBoot(void);
// Every nonzero action forbids a gameplay iteration. Exported requires the
// caller to reset its pacer before continuing the outer loop.
enum { MK64_LAB_ITERATE = 0, MK64_LAB_TERMINAL = 1, MK64_LAB_EXIT = 2, MK64_LAB_EXPORTED = 3 };
int Mk64Lab3DSTick(void);
void Mk64Lab3DSDumpComplete(const char* directory, bool success);
bool Mk64Lab3DSWriteContext(const char* directory);
const char* Mk64Diagnostics3DSLabDirectory(void);
bool Mk64Diagnostics3DSRequestLabDump(void);
bool Mk64Diagnostics3DSLabFlush(void);
#ifdef __cplusplus
}
#endif
