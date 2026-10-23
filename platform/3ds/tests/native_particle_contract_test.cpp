#include "native_particle_contract_3ds.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <initializer_list>
#include <cstring>
int main() {
    Mk64NativeParticle3DS p{};float model[16]{};model[0]=model[5]=model[10]=model[15]=1;
    assert(!Mk64NativeParticleValid(&p,MK64_PARTICLE_QUAD_32));
    Mk64NativeParticleSetModel(&p,model);
    Mk64NativeParticleSetImage(&p,"synthetic-smoke",MK64_PARTICLE_INTENSITY8,32,32);
    Mk64NativeParticleLerp(&p,-1,256,511,-257,513,1024,300);
    assert(p.primitive[0]==255 && p.primitive[1]==0 && p.primitive[2]==255 && p.primitive[3]==44);
    assert(p.environment[0]==255 && p.environment[1]==1 && p.environment[2]==0 && p.environment[3]==255);
    assert(p.color==MK64_PARTICLE_COLOR_LERP);
    assert(Mk64NativeParticleValid(&p,MK64_PARTICLE_QUAD_32));
    assert(!Mk64NativeParticleValid(&p,MK64_PARTICLE_EXPLICIT_QUAD));
    p.vertices=model;assert(Mk64NativeParticleValid(&p,MK64_PARTICLE_EXPLICIT_QUAD));
    assert(!Mk64NativeParticleValid(&p,unsigned(-1)));
    assert(!Mk64NativeParticleValid(&p,256));
    auto saved=p;
    for(unsigned side:{0u,65u,65536u}) {
        Mk64NativeParticleSetImage(&p,"synthetic-smoke",MK64_PARTICLE_INTENSITY8,side,32);
        assert(!Mk64NativeParticleValid(&p,MK64_PARTICLE_QUAD_32));p=saved;
    }
    Mk64NativeParticleSetImage(&p,"synthetic-smoke",256,32,32);
    assert(!Mk64NativeParticleValid(&p,MK64_PARTICLE_QUAD_32));p=saved;
    Mk64NativeParticleSetSetup(&p,256);
    assert(!Mk64NativeParticleValid(&p,MK64_PARTICLE_QUAD_32));p=saved;
    Mk64NativeParticleSetSetup(&p,MK64_PARTICLE_GRASS_SETUP);
    assert(p.compare==MK64_PARTICLE_NO_COMPARE && p.raster==MK64_PARTICLE_EDGE_RASTER);
    const auto flags=p;Mk64NativeParticleTint(&p,1,2,3,4);
    assert(p.color==MK64_PARTICLE_TEXTURE_TINT && p.compare==flags.compare && p.raster==flags.raster);
    for(uint32_t invalid:{0x7f800000u,0xff800000u,0x7fc00000u}) {
        // Inject foreign bytes rather than evaluating nonfinite FP literals
        // under the producer's finite-math-only compiler contract.
        p=saved;volatile uint32_t source=invalid;const uint32_t bits=source;
        std::memcpy(p.model+7,&bits,4);
        assert(!Mk64NativeParticleValid(&p,MK64_PARTICLE_QUAD_32));
    }
    assert(!Mk64NativeParticleValid(nullptr,MK64_PARTICLE_QUAD_32));
}
