#pragma once

#include <cstddef>
#include <cfloat>

#ifndef IM_ARRAYSIZE
#define IM_ARRAYSIZE(array) (static_cast<int>(sizeof(array) / sizeof((array)[0])))
#endif

#ifndef ICON_FA_UNDO
#define ICON_FA_UNDO ""
#endif

#ifndef SPDLOG_ERROR
#define SPDLOG_ERROR(...) ((void)0)
#endif

struct ImFont;

// Editor-only methods are compiled out by the linker, but a few stock engine
// translation units define their editor property panels alongside gameplay.
// These no-op declarations keep that code independent of Dear ImGui on 3DS.
namespace ImGui {
inline void Text(const char*, ...) {}
inline void SameLine(float = 0.0f, float = -1.0f) {}
inline void Separator() {}
inline void SetNextItemWidth(float) {}
inline bool Button(const char*) { return false; }
inline bool Checkbox(const char*, bool*) { return false; }
inline bool ColorEdit4(const char*, float*) { return false; }
inline bool Combo(const char*, int*, const char* const*, int, int = -1) { return false; }
inline bool DragFloat(const char*, float*, float = 1.0f, float = 0.0f, float = 0.0f,
                      const char* = "%.3f", int = 0) { return false; }
inline bool DragFloat2(const char*, float*, float = 1.0f, float = 0.0f, float = 0.0f,
                       const char* = "%.3f", int = 0) { return false; }
inline bool DragFloat3(const char*, float*, float = 1.0f, float = 0.0f, float = 0.0f,
                       const char* = "%.3f", int = 0) { return false; }
inline bool DragInt2(const char*, int*, float = 1.0f, int = 0, int = 0,
                     const char* = "%d", int = 0) { return false; }
inline bool DragInt3(const char*, int*, float = 1.0f, int = 0, int = 0,
                     const char* = "%d", int = 0) { return false; }
inline bool InputInt(const char*, int*, int = 1, int = 100, int = 0) { return false; }
inline bool InputInt(const char*, long*, int = 1, int = 100, int = 0) { return false; }
inline bool InputText(const char*, char*, std::size_t, int = 0, void* = nullptr,
                      void* = nullptr) { return false; }
}
