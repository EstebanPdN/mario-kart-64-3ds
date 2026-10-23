#pragma once
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Source-independent particle description. Rendering consumes this data, not
// GBI commands. The game adapter retains transforms, simulation and ordering.
typedef enum {
    MK64_PARTICLE_RGBA5551, MK64_PARTICLE_INTENSITY8,
    MK64_PARTICLE_INTENSITY_ALPHA8
} Mk64NativeParticleFormat;
typedef enum { MK64_PARTICLE_SMOKE_SETUP, MK64_PARTICLE_GRASS_SETUP } Mk64NativeParticleSetup;
typedef enum { MK64_PARTICLE_TEXTURE_TINT, MK64_PARTICLE_COLOR_LERP } Mk64NativeParticleColor;
typedef enum { MK64_PARTICLE_INHERIT_COMPARE, MK64_PARTICLE_NO_COMPARE,
    MK64_PARTICLE_THRESHOLD_COMPARE, MK64_PARTICLE_DITHER_COMPARE } Mk64NativeParticleCompare;
typedef enum { MK64_PARTICLE_INHERIT_RASTER, MK64_PARTICLE_BLEND_RASTER,
    MK64_PARTICLE_EDGE_RASTER } Mk64NativeParticleRaster;
typedef enum { MK64_PARTICLE_QUAD_32, MK64_PARTICLE_QUAD_16,
    MK64_PARTICLE_QUAD_REVERSED_32, MK64_PARTICLE_EXPLICIT_QUAD } Mk64NativeParticleQuad;

typedef struct {
    float model[16];
    const char* image;
    const void* vertices;
    uint8_t primitive[4],environment[4];
    uint16_t width,height;
    uint8_t format,setup,color,compare,raster;
    uint8_t transformReady,imageReady,colorReady;
} Mk64NativeParticle3DS;

static inline void Mk64NativeParticleSetModel(Mk64NativeParticle3DS* p,const float* matrix) {
    memcpy(p->model,matrix,sizeof(p->model));p->transformReady=1;
}
static inline void Mk64NativeParticleSetSetup(Mk64NativeParticle3DS* p,unsigned setup) {
    p->setup=(unsigned)setup<=MK64_PARTICLE_GRASS_SETUP?(uint8_t)setup:255;
    if(setup==MK64_PARTICLE_GRASS_SETUP) {
        p->compare=MK64_PARTICLE_NO_COMPARE;p->raster=MK64_PARTICLE_EDGE_RASTER;
    }
}
static inline void Mk64NativeParticleSetImage(Mk64NativeParticle3DS* p,const char* image,
        unsigned format,unsigned width,unsigned height) {
    p->image=image;p->format=(uint8_t)format;p->width=(uint16_t)width;p->height=(uint16_t)height;
    p->imageReady=width>0 && width<=64 && height>0 && height<=64 && image!=0 &&
        (unsigned)format<=MK64_PARTICLE_INTENSITY_ALPHA8;
}
static inline void Mk64NativeParticleTint(Mk64NativeParticle3DS* p,int r,int g,int b,int a) {
    p->color=MK64_PARTICLE_TEXTURE_TINT;
    p->primitive[0]=(uint8_t)r;p->primitive[1]=(uint8_t)g;p->primitive[2]=(uint8_t)b;p->primitive[3]=(uint8_t)a;
    p->colorReady=1;
}
static inline void Mk64NativeParticleLerp(Mk64NativeParticle3DS* p,int r,int g,int b,
        int er,int eg,int eb,int a) {
    Mk64NativeParticleTint(p,r,g,b,a);p->color=MK64_PARTICLE_COLOR_LERP;
    p->environment[0]=(uint8_t)er;p->environment[1]=(uint8_t)eg;
    p->environment[2]=(uint8_t)eb;p->environment[3]=255;
}
static inline int Mk64NativeParticleValid(const Mk64NativeParticle3DS* p,unsigned quad) {
    if(!(p && p->transformReady==1 && p->imageReady==1 && p->colorReady==1 &&
        p->format<=MK64_PARTICLE_INTENSITY_ALPHA8 && p->setup<=MK64_PARTICLE_GRASS_SETUP &&
        p->color<=MK64_PARTICLE_COLOR_LERP && p->compare<=MK64_PARTICLE_DITHER_COMPARE &&
        p->raster<=MK64_PARTICLE_EDGE_RASTER && (unsigned)quad<=MK64_PARTICLE_EXPLICIT_QUAD &&
        p->width && p->width<=64 && p->height && p->height<=64 && p->image &&
        (quad!=MK64_PARTICLE_EXPLICIT_QUAD || p->vertices)))return 0;
    // Remain valid under the game producer's fast-math compilation. The
    // renderer still performs its own checked fixed-point quantization.
    for(unsigned i=0;i<16;++i) {
        uint32_t bits;memcpy(&bits,p->model+i,sizeof(bits));
        if((bits&0x7f800000u)==0x7f800000u)return 0;
    }
    return 1;
}
// Implemented by the ordered renderer integration, not by the game adapter.
void Mk64NativeParticleEmit(const void* anchor,const Mk64NativeParticle3DS* particle,
                          Mk64NativeParticleQuad quad);
#ifdef __cplusplus
}
#endif
