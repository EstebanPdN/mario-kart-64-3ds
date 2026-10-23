#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
namespace Fast {class Interpreter;}
extern "C" void Mk64NativeFrameReplay(Fast::Interpreter*,const void* root);
extern "C" {
#endif
void Mk64NativeFrameBegin(void);
int Mk64NativeListStopped(void);
#ifdef __cplusplus
}
#endif
