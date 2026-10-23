#include "native_particle_pack_3ds.hpp"
#include <cassert>
#include <vector>
#include <fstream>
#include <iterator>
using namespace mk64_3ds::native_scene;
int main(int argc,char** argv) {
    if(argc==2) {
        std::ifstream file(argv[1],std::ios::binary);assert(file);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
        ParticlePackView view;assert(ValidateParticlePack(bytes.data(),bytes.size(),view));
        return 0;
    }
    const char name[]="synthetic-effect";
    std::vector<uint8_t> bytes(32+24+192+sizeof(name),0);
    auto write=[&](size_t at,uint32_t value){for(unsigned i=0;i<4;++i)bytes[at+i]=value>>(i*8);};
    std::memcpy(bytes.data(),"NPF1",4);
    write(4,1);write(8,1);write(12,3);write(16,32);write(20,56);write(24,248);write(28,bytes.size());
    ParticleImage image{ParticleImageIdentity(name,sizeof(name)-1),248,sizeof(name),1,0,32,32,0};
    std::memcpy(bytes.data()+32,&image,sizeof(image));std::memcpy(bytes.data()+248,name,sizeof(name));
    ParticlePackView view;assert(ValidateParticlePack(bytes.data(),bytes.size(),view));
    for(size_t n=0;n<bytes.size();++n){assert(!ValidateParticlePack(bytes.data(),n,view));assert(!view.images&&!view.imageCount);}
    const auto saved=bytes;
    for(size_t at:{size_t(0),size_t(4),size_t(8),size_t(12),size_t(16),size_t(20),size_t(24),size_t(28),
                   size_t(32),size_t(40),size_t(44),size_t(48),size_t(52)}) {
        write(at,0xffffffff);assert(!ValidateParticlePack(bytes.data(),bytes.size(),view));bytes=saved;
    }
    for(size_t at:{size_t(46),size_t(47),size_t(70),size_t(248),bytes.size()-1}) {
        bytes[at]^=0xff;assert(!ValidateParticlePack(bytes.data(),bytes.size(),view));bytes=saved;
    }
    bytes[250]=0;assert(!ValidateParticlePack(bytes.data(),bytes.size(),view));bytes=saved;
    bytes.insert(bytes.begin(),0);assert(ValidateParticlePack(bytes.data()+1,saved.size(),view));
    assert(!ValidateParticlePack(nullptr,saved.size(),view));
}
