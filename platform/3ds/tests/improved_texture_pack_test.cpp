#include "improved_texture_pack_3ds.hpp"
#include <cassert>
#include <cstring>
#include <fstream>

static int releases=0;
static void Released(const void* address) { assert(address);++releases; }

static void Put(std::vector<uint8_t>& out,size_t p,uint64_t value,unsigned bytes) {
    for (unsigned i=0;i<bytes;++i) out[p+i]=uint8_t(value>>(i*8));
}
static std::vector<uint8_t> Fixture() {
    const std::array<uint8_t,16> pixels={255,0,0,255,0,255,0,128,0,0,255,0,255,255,255,255};
    std::vector<uint8_t> compressed(compressBound(pixels.size()));uLongf size=compressed.size();
    assert(compress2(compressed.data(),&size,pixels.data(),pixels.size(),9)==Z_OK);compressed.resize(size);
    std::vector<uint8_t> out(48+size);std::memcpy(out.data(),"MUI3",4);
    Put(out,4,1,4);Put(out,8,1,4);Put(out,12,32,4);
    Put(out,16,mk64_3ds::improved::Hash("textures/karts/test"),8);
    Put(out,24,48,4);Put(out,28,size,4);Put(out,32,crc32(0,pixels.data(),pixels.size()),4);
    for (size_t p:{36U,38U,40U,42U})Put(out,p,2,2);
    std::copy(compressed.begin(),compressed.end(),out.begin()+48);return out;
}
static void Save(const std::vector<uint8_t>& bytes) {
    std::ofstream file("native-pack-test.bin",std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
}
int main() {
    using namespace mk64_3ds::improved;
    Pack pack;pack.SetReleaseCallback(Released);assert(!pack.Open("missing-native-pack.bin"));
    auto good=Fixture();Save(good);assert(pack.Open("native-pack-test.bin"));
    assert(!pack.Load("missing",2,2));
    auto image=pack.Load("__OTR__textures/karts/test",2,2,true);assert(image && image.width==2 && image.height==2);
    assert(static_cast<uint8_t>((*image.pixels)[7])==128);
    assert(pack.Load("textures/karts/test",2,2,true).pixels==image.pixels);
    assert(pack.CachedBytes()==16);pack.Close();assert(pack.CachedBytes()==0);
    assert(image && static_cast<uint8_t>((*image.pixels)[7])==128); // Last borrower owns its bytes.
    assert(releases==0);
    image={};assert(releases==1); // Invalidate renderer identity on final release only.
    for (const auto [offset,value,bytes]:{std::array<uint64_t,3>{4,2,4},{8,20001,4},
            {12,16,4},{24,47,4},{28,2097153,4},{36,513,2},{38,0,2},{40,0,2}}) {
        auto broken=good;Put(broken,offset,value,bytes);Save(broken);assert(!pack.Open("native-pack-test.bin"));
    }
    auto badCrc=good;badCrc[32]^=1;Save(badCrc);assert(pack.Open("native-pack-test.bin"));assert(!pack.Load("textures/karts/test",2,2));
    for (size_t length=0;length<good.size();++length) {
        Save(std::vector<uint8_t>(good.begin(),good.begin()+length));assert(!pack.Open("native-pack-test.bin"));
    }
    std::remove("native-pack-test.bin");
    std::puts("PASS: native pack bounds, truncated streams, CRC corruption, optional fallback, original path normalization and scoped cache ownership.");
}
