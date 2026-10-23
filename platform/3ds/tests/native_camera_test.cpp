#include "native_camera_3ds.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

using namespace mk64_3ds::native_scene;
int main(int argc,char** argv) {
    if (argc==2) {
        std::ifstream f(argv[1],std::ios::binary);assert(f);
        std::vector<char> bytes((std::istreambuf_iterator<char>(f)),{});
        assert(bytes.size()%132==0 && !bytes.empty());
        for (size_t at=0;at<bytes.size();at+=132) {
            float input[16],scale;Camera expected,actual;
            std::memcpy(input,bytes.data()+at,64);std::memcpy(&scale,bytes.data()+at+64,4);
            std::memcpy(&expected,bytes.data()+at+68,64);
            assert(PrepareCamera(input,scale,actual));
            assert(!std::memcmp(&expected,&actual,sizeof actual));
        }
        return 0;
    }
    const float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    Camera camera;
    assert(PrepareCamera(identity,.8f,camera));
    assert(camera.rows[0][1]==1 && camera.rows[1][0]==-.8f);
    assert(camera.rows[2][2]==.5f && camera.rows[2][3]==-.5f && camera.rows[3][3]==1);
    for (float z:{-1.f,0.f,1.f}) {
        const float depth=camera.rows[2][2]*z+camera.rows[2][3];
        assert(depth==(z-1.f)*.5f);
    }
    for (float bad:{0.f,-1.f,9.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        assert(!PrepareCamera(identity,bad,camera));
        for (auto& row:camera.rows) for (float v:row) assert(v==0);
    }
    float invalid[16];std::memcpy(invalid,identity,64);invalid[10]=std::numeric_limits<float>::quiet_NaN();
    assert(!PrepareCamera(invalid,1,camera));
    std::memset(invalid,0,64);assert(!PrepareCamera(invalid,1,camera));
    assert(!PrepareCamera(nullptr,1,camera));
    CameraUniformCache uniforms;
    Camera first{};first.rows[0][0]=1;
    assert(uniforms.Update(first));assert(!uniforms.Update(first));
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c) {
        auto changed=first;changed.rows[r][c]+=0.25f;
        assert(uniforms.Update(changed));assert(!uniforms.Update(changed));
        assert(uniforms.Update(first));
    }
    // A shader/batch transition invalidates even an unchanged matrix.
    uniforms.Invalidate();assert(uniforms.Update(first));assert(!uniforms.Update(first));
}
