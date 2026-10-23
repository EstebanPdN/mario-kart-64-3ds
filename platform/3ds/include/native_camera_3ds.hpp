#pragma once

#include <cmath>
#include <cstring>

namespace mk64_3ds::native_scene {

struct Camera { float rows[4][4]{}; };
static_assert(sizeof(Camera)==64);

// One native shader owner and batch only. A foreign shader can overwrite the
// same uniform registers, so every BeginBatch invalidates this cache.
class CameraUniformCache {
public:
    void Invalidate(){valid_=false;}
    bool Update(const Camera& camera) {
        if(valid_ && std::memcmp(&camera_,&camera,sizeof camera)==0)return false;
        camera_=camera;valid_=true;return true;
    }
private:
    Camera camera_{};bool valid_=false;
};

// Prepare once per camera update, not once per vertex. Input is a row-vector
// world-to-clip matrix with conventional [-w,+w] depth. Compile its depth row
// into PICA's [-w,0] range before float24 upload, avoiding nearly equal Z/W
// subtraction in the shader. The final X/Y rotation matches the physical top
// framebuffer. Game-specific view/projection construction stays in the adapter.
inline bool PrepareCamera(const float input[16],float horizontalScale,Camera& result) {
    result={};
    if (!input || !std::isfinite(horizontalScale) || horizontalScale<=0 || horizontalScale>8) return false;
    bool hasW=false;
    for (unsigned i=0;i<16;++i) {
        if (!std::isfinite(input[i]) || std::fabs(input[i])>0x1p30f) return false;
        if ((i&3)==3 && input[i]!=0) hasW=true;
    }
    if (!hasW) return false;
    Camera camera;
    for (unsigned i=0;i<4;++i) {
        camera.rows[0][i]=input[i*4+1];
        camera.rows[1][i]=-(horizontalScale*input[i*4]);
        camera.rows[2][i]=static_cast<float>((double(input[i*4+2])-double(input[i*4+3]))*.5);
        camera.rows[3][i]=input[i*4+3];
    }
    result=camera;
    return true;
}

} // namespace mk64_3ds::native_scene
