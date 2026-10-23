#pragma once
#include <cstddef>
#include <cstdint>
namespace Fast { class Interpreter; class GfxRenderingAPICitro3D; }
// Development-only matched replay. Never compile this into the normal build.
void Mk64NativeComparisonRun(Fast::Interpreter*,Fast::GfxRenderingAPICitro3D*,
    void* commands,size_t bytes,uint64_t frame);
void Mk64NativeComparisonSave(unsigned frame);
extern "C" void Mk64NativeCourseUseReference(bool enabled);
extern "C" uint64_t Mk64NativeCourseDrawCount();
