#include "native_material_3ds.hpp"
#include "depth_precision_3ds.hpp"

namespace mk64_3ds::native_scene {

namespace {
GPU_TEXTURE_WRAP_PARAM Wrap(std::uint32_t value) {
    return value&2 ? GPU_CLAMP_TO_EDGE : (value&1 ? GPU_MIRRORED_REPEAT : GPU_REPEAT);
}
}

bool MaterialBinder::Apply(const Material& material,C3D_Tex* textures,const RasterState* raster) {
    if (material.cull==3) return false;
    const bool alphaChanged=!valid_ || bool(raster)!=previousHadRaster_ ||
        (raster && (raster->alphaTest!=previousRaster_.alphaTest ||
         raster->alphaReference!=previousRaster_.alphaReference || raster->blend!=previousRaster_.blend ||
         raster->alphaOutput!=previousRaster_.alphaOutput));
    if (!valid_ || previous_.kind!=material.kind || previous_.cutout!=material.cutout || alphaChanged) {
        for (int i=0;i<6;++i) C3D_TexEnvInit(C3D_GetTexEnv(i));
        auto* env=C3D_GetTexEnv(0);
        if (material.kind==static_cast<std::uint32_t>(MaterialKind::Shade)) {
            C3D_TexEnvSrc(env,C3D_RGB,GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(env,C3D_RGB,GPU_REPLACE);
        } else if(material.kind==static_cast<std::uint32_t>(MaterialKind::Texture) ||
                  material.kind==static_cast<std::uint32_t>(MaterialKind::TextureShadeAlpha)) {
            C3D_TexEnvSrc(env,C3D_RGB,GPU_TEXTURE0);
            C3D_TexEnvFunc(env,C3D_RGB,GPU_REPLACE);
        } else if(material.kind==static_cast<std::uint32_t>(MaterialKind::DecalRgba)) {
            C3D_TexEnvSrc(env,C3D_RGB,GPU_TEXTURE0,GPU_PRIMARY_COLOR,GPU_TEXTURE0);
            C3D_TexEnvOpRgb(env,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_ALPHA);
            C3D_TexEnvFunc(env,C3D_RGB,GPU_INTERPOLATE);
        } else {
            C3D_TexEnvSrc(env,C3D_RGB,GPU_TEXTURE0,GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(env,C3D_RGB,GPU_MODULATE);
        }
        const bool usesAlpha=raster ? raster->alphaOutput : material.cutout!=0;
        if (!usesAlpha) {
            C3D_TexEnvSrc(env,C3D_Alpha,GPU_CONSTANT);
            C3D_TexEnvFunc(env,C3D_Alpha,GPU_REPLACE);
        } else if (material.kind==static_cast<std::uint32_t>(MaterialKind::ModulateRgba)) {
            C3D_TexEnvSrc(env,C3D_Alpha,GPU_TEXTURE0,GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(env,C3D_Alpha,GPU_MODULATE);
        } else {
            const bool shadeAlpha=material.kind==static_cast<std::uint32_t>(MaterialKind::Shade) ||
                material.kind==static_cast<std::uint32_t>(MaterialKind::ModulateRgbShadeAlpha) ||
                material.kind==static_cast<std::uint32_t>(MaterialKind::TextureShadeAlpha) ||
                material.kind==static_cast<std::uint32_t>(MaterialKind::DecalRgba);
            C3D_TexEnvSrc(env,C3D_Alpha,shadeAlpha?GPU_PRIMARY_COLOR:GPU_TEXTURE0);
            C3D_TexEnvFunc(env,C3D_Alpha,GPU_REPLACE);
        }
        const bool test=raster ? raster->alphaTest : (material.cutout==1 || material.cutout==2);
        const bool blend=raster ? raster->blend : material.cutout>=2;
        C3D_AlphaTest(test,GPU_GREATER,raster?raster->alphaReference:(material.cutout==1?48:8));
        C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,blend?GPU_SRC_ALPHA:GPU_ONE,
            blend?GPU_ONE_MINUS_SRC_ALPHA:GPU_ZERO,GPU_ONE,blend?GPU_ONE_MINUS_SRC_ALPHA:GPU_ZERO);
    }
    if(raster && (!valid_ || !previousHadRaster_ || raster->depthTest!=previousRaster_.depthTest ||
        raster->depthWrite!=previousRaster_.depthWrite || raster->decal!=previousRaster_.decal)) {
        C3D_DepthMap(true,-1.f,mk64_3ds::PicaDecalBias(raster->decal));
        C3D_DepthTest(raster->depthTest,raster->depthTest?(raster->decal?GPU_GEQUAL:GPU_GREATER):GPU_ALWAYS,
            static_cast<GPU_WRITEMASK>(GPU_WRITE_COLOR|(raster->depthWrite?GPU_WRITE_DEPTH:0)));
    }
    if (!valid_ || material.cull!=previous_.cull) {
        C3D_CullFace(material.cull==2 ? GPU_CULL_BACK_CCW :
                     (material.cull==1 ? GPU_CULL_FRONT_CCW : GPU_CULL_NONE));
    }
    const bool selectedLinear=raster?raster->linearFilter:linear_;
    const bool previousLinear=previousHadRaster_?previousRaster_.linearFilter:linear_;
    if (!valid_ || material.texture!=previous_.texture || material.wrapS!=previous_.wrapS || material.wrapT!=previous_.wrapT || selectedLinear!=previousLinear) {
        auto& texture=textures[material.texture];
        C3D_TexSetFilter(&texture,selectedLinear?GPU_LINEAR:GPU_NEAREST,selectedLinear?GPU_LINEAR:GPU_NEAREST);
        C3D_TexSetWrap(&texture,Wrap(material.wrapS),Wrap(material.wrapT));
        C3D_TexBind(0,&texture);
    }
    if (!valid_ || std::memcmp(material.uvScaleOffset,previous_.uvScaleOffset,sizeof material.uvScaleOffset)) {
        const auto* u=material.uvScaleOffset;
        C3D_FVUnifSet(GPU_VERTEX_SHADER,uvUniform_,u[0],u[1],u[2],u[3]);
    }
    if (!valid_ || std::memcmp(material.uvClamp,previous_.uvClamp,sizeof material.uvClamp)) {
        C3D_FVUnifSet(GPU_VERTEX_SHADER,clampUniform_,material.uvClamp[0],material.uvClamp[1],0,0);
    }
    previous_=material;valid_=true;
    previousHadRaster_=raster!=nullptr;if(raster)previousRaster_=*raster;
    return true;
}

} // namespace mk64_3ds::native_scene
