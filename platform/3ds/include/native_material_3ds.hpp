#pragma once

#include "native_scene_format_3ds.hpp"
#include <citro3d.h>

namespace mk64_3ds::native_scene {

// Non-owning state cache for a caller-owned shader/frame. Reset after another
// renderer changes Citro3D state. UV uniforms belong to this shader instance.
class MaterialBinder {
public:
    MaterialBinder(int uvUniform,int clampUniform) : uvUniform_(uvUniform),clampUniform_(clampUniform) {}
    void Reset() { valid_=false; }
    void SetLinearFiltering(bool linear) { if(linear_!=linear){linear_=linear;valid_=false;} }
    // Inputs are validated at cache load. NLC3/4 calls inherit caller depth;
    // NLC5 supplies explicit raster state. Restore caller state when leaving
    // a raster-owned scene scope. False means CullBoth emits no geometry.
    bool Apply(const Material& material,C3D_Tex* textures,const RasterState* raster=nullptr);
private:
    int uvUniform_,clampUniform_;
    bool valid_=false;
    bool linear_=true;
    Material previous_{};
    RasterState previousRaster_{};
    bool previousHadRaster_=false;
};

} // namespace mk64_3ds::native_scene
