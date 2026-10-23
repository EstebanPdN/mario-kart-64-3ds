#pragma once

#include "native_mesh_format_3ds.hpp"
#include "native_raster_state_3ds.hpp"
#include <cmath>

namespace mk64_3ds::native_scene {

enum class MaterialKind : std::uint32_t {
    Shade, ModulateRgba, ModulateRgbTextureAlpha,
    ModulateRgbShadeAlpha, Texture, TextureShadeAlpha, DecalRgba
};
enum class MaterialAlpha : std::uint32_t { Opaque, Cutout, ThresholdBlend, Blend };
struct Material {
    std::uint32_t texture, kind, cull, cutout;
    float uvScaleOffset[4];
    float uvClamp[2];
    std::uint32_t wrapS, wrapT;
};
struct DrawRange { std::uint32_t first, count, material, reserved; };
struct View { std::uint32_t firstReference, referenceCount, kind, mirrored; };
struct TextureInfo { std::uint32_t width, height, bytes; };
static_assert(sizeof(Material) == 48 && sizeof(DrawRange) == 16);
static_assert(sizeof(View) == 16 && sizeof(TextureInfo) == 12);

struct SceneView {
    const std::uint8_t *textures = nullptr, *materials = nullptr, *ranges = nullptr;
    const std::uint8_t *views = nullptr, *references = nullptr;
    std::uint32_t textureCount = 0, materialCount = 0, rangeCount = 0;
    std::uint32_t viewCount = 0, referenceCount = 0;
    std::uint32_t materialStride = sizeof(Material);
};

// Startup-only validation. All table views borrow the cache owner. The mesh's
// validated index count supplies the cross-file range limit. Drawing consumes
// typed native records prepared from this view, never source command words.
inline bool ValidateScene(const void* bytes, std::size_t size,
                          std::uint32_t meshIndexCount, SceneView& result) {
    result = {};
    if (!bytes || size < 32 || size > 8u*1024u*1024u) return false;
    const auto* p = static_cast<const std::uint8_t*>(bytes);
    const bool version3=std::memcmp(p,"NLC3",4)==0 && ReadLE32(p+4)==3;
    const bool version4=std::memcmp(p,"NLC4",4)==0 && ReadLE32(p+4)==4;
    const bool version5=std::memcmp(p,"NLC5",4)==0 && ReadLE32(p+4)==5;
    if ((!version3 && !version4 && !version5) || ReadLE32(p+28)) return false;
    const std::size_t materialStride=sizeof(Material)+(version5?sizeof(RasterState):0);
    const auto textures = ReadLE32(p+8), materials = ReadLE32(p+12);
    const auto ranges = ReadLE32(p+16), views = ReadLE32(p+20), refs = ReadLE32(p+24);
    if (!textures || textures > 512 || !materials || materials > 4096 ||
        !ranges || ranges > 65535 || !views || views > 4096 || !refs || refs > 1048576) return false;
    const std::size_t expected = 32u + std::size_t(textures)*12u +
        std::size_t(materials)*materialStride + std::size_t(ranges)*16u + std::size_t(views)*16u + std::size_t(refs)*2u;
    if (expected != size) return false;
    SceneView view;
    view.textures=p+32; view.materials=view.textures+textures*12u;
    view.ranges=view.materials+materials*materialStride; view.views=view.ranges+ranges*16u;
    view.references=view.views+views*16u;
    for (std::uint32_t i=0;i<textures;++i) {
        const auto* t=view.textures+i*12u;
        const auto w=ReadLE32(t),h=ReadLE32(t+4);
        if (w<8 || h<8 || w>512 || h>512 || (w&(w-1)) || (h&(h-1)) || ReadLE32(t+8)!=w*h*4u) return false;
    }
    for (std::uint32_t i=0;i<materials;++i) {
        Material material;
        std::memcpy(&material,view.materials+i*materialStride,sizeof material);
        if (material.texture>=textures || material.kind>(version5?6u:2u) || material.cull>3 || material.cutout>((version4||version5)?3u:1u) ||
            material.wrapS>3 || material.wrapT>3) return false;
        if(version5 && material.cutout)return false; // RasterState owns all alpha choices.
        for (float value:material.uvScaleOffset) if (!std::isfinite(value) || std::fabs(value)>0x1p30f) return false;
        for (float value:material.uvClamp) if (!std::isfinite(value) || value<0 || value>0x1p30f) return false;
        if(version5) {
            RasterState raster;
            std::memcpy(&raster,view.materials+i*materialStride+sizeof(Material),sizeof raster);
            if(!ValidRasterState(raster))return false;
        }
    }
    for (std::uint32_t i=0;i<ranges;++i) {
        const auto* r=view.ranges+i*16u;
        const auto first=ReadLE32(r),count=ReadLE32(r+4);
        if (!count || first%3 || count%3 || first>meshIndexCount || count>meshIndexCount-first ||
            ReadLE32(r+8)>=materials || ReadLE32(r+12)) return false;
    }
    for (std::uint32_t i=0;i<views;++i) {
        const auto* v=view.views+i*16u;
        const auto first=ReadLE32(v),count=ReadLE32(v+4);
        if (first>refs || count>refs-first || ReadLE32(v+8)>2 || ReadLE32(v+12)>1) return false;
    }
    for (std::uint32_t i=0;i<refs;++i) {
        const auto reference=std::uint32_t(view.references[2*i]) | (std::uint32_t(view.references[2*i+1])<<8);
        if (reference>=ranges) return false;
    }
    view.textureCount=textures; view.materialCount=materials; view.rangeCount=ranges;
    view.viewCount=views; view.referenceCount=refs; result=view;
    result.materialStride=materialStride;
    return true;
}

} // namespace mk64_3ds::native_scene
