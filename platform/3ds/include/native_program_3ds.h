#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
namespace Fast { class Interpreter; }
extern "C" bool Mk64NativeProgramDraw(Fast::Interpreter*,uint64_t);
extern "C" {
#endif
bool Mk64NativeProgramMutableDependency(uint64_t hash);
void Mk64NativeProgramResetCourseChanges(void);
void Mk64NativeProgramDisable(uintptr_t resource);
void Mk64NativeProgramReleaseWorkingSet(void);
void Mk64NativeProgramPreload(const char* course);
void Mk64NativeProgramPrepareMode(void);
// Mutation producers publish native values alongside their compatibility data.
void Mk64NativeProgramSetMirror(const char* resource,int swapped);
void Mk64NativeProgramResetMirrors(void);
void Mk64NativeProgramSetTileBounds(uintptr_t resource,int s,int t);
#ifdef __cplusplus
}
#endif
