#pragma once
#include "native_sprite_pack_3ds.hpp"
#include "native_original_tiles_3ds.hpp"
#include "native_camera_3ds.hpp"
#include "native_sprite_batch_3ds.hpp"
#include <citro3d.h>
#include <string>

namespace mk64_3ds::native_scene {
struct SpriteStatistics {
    uint64_t hits=0,misses=0,draws=0,uploadBytes=0;
    uint64_t batches=0,materialChanges=0,textureBinds=0;
    uint64_t geometryCleanCalls=0,geometryCleanBytes=0;
};
class GpuSprites {
public:
    using Decoder=bool (*)(const uint8_t*,size_t,uint8_t*,size_t,uint32_t);
    ~GpuSprites();
    bool Load(const char* path,Decoder decoder);
    // Immutable effect images share the native quad backend without allocating
    // the character animation cache or opening a kart pack.
    bool LoadEffects(Decoder decoder);
    // Startup-only, pre-tiled immutable textures. No slot can be replaced while
    // queued GPU work might still refer to it. Keys occupy a separate namespace.
    bool LoadStatic(const char* path,uint32_t& key);
    // Only call after the renderer's GPU-completion fence, before new draws.
    void BeginAfterCompletion();
    bool Resolve(const char* name,const uint8_t* palette,bool improved,uint32_t& key) const;
    bool PrepareTextureMode(bool improved,size_t budget,bool loadingRetry=false);
    bool TextureModePrepared(bool improved)const{return preparedMode_==(improved?1:0);}
    bool DropResident(){const bool raw=originalTiles_.Drop();return pack_.DropResident() || raw;}
    size_t ResidentBytes()const{return pack_.ResidentBytes()+originalTiles_.ResidentBytes();}
    size_t ResidentHeapBytes()const{return pack_.ResidentHeapBytes()+originalTiles_.HeapBytes();}
    // The caller guarantees no foreign GPU state changes except viewport/depth
    // between draws. The backend defers Fast3D projection uploads until this
    // native scope ends; PICA uniform registers are shared between programs.
    void BeginBatch();
    // Publish all geometry written in this scope before any GPU queue
    // submission/split. Every native batch exit, including exceptions, calls it.
    void EndBatch();
    void Draw(uint32_t key,const Vertex vertices[8],const Camera& camera,const SpriteStyle& style);
    const SpriteStatistics& Statistics()const{return statistics_;}
private:
    static constexpr unsigned kSlots=128,kPackets=64;
    bool Initialize(unsigned packets,bool characterCache);
    SpritePack pack_;SpriteResidency<kSlots> residency_;Decoder decode_=nullptr;
    OriginalSpriteTiles originalTiles_;std::string originalTilesPath_;
    std::array<C3D_Tex,kSlots> textures_{};unsigned allocatedTextures_=0;
    std::array<C3D_Tex,64> staticTextures_{};unsigned allocatedStatic_=0;
    Vertex* vertices_=nullptr;uint16_t* indices_=nullptr;unsigned usedVertices_=0,usedIndices_=0;
    unsigned packetCapacity_=0;
    bool deferGeometryCleaning_=false;
    unsigned flushedVertices_=0,flushedIndices_=0;
    bool batchBound_=false,materialValid_=false;
    CameraUniformCache matrixCache_;
    SpriteStyle previousStyle_{};const C3D_Tex* boundTexture_=nullptr;
    DVLB_s* binary_=nullptr;shaderProgram_s program_{};bool programReady_=false,ready_=false;
    int matrixUniform_=-1,uvUniform_=-1,clampUniform_=-1;
    uint8_t boundLogicalWidth_=0,boundLogicalHeight_=0;bool boundClampLogical_=false;
    std::array<uint8_t,17408> packed_{};
    std::vector<uint8_t> oversizedPacked_;
    SpritePageCache<16> pageCache_;int preparedMode_=-1;
    SpriteStatistics statistics_{};
};
}
