#include "native_dynamic_texture_3ds.hpp"
#include <cassert>
#include <array>
#include <cstdio>
using namespace mk64_3ds::native_scene;
int main() {
    std::vector<uint8_t> table(36);std::memcpy(table.data(),"NMD1",4);
    uint32_t header[]={1,1,32};std::memcpy(table.data()+4,header,sizeof header);
    DynamicTextureRecord row{1,64,32,64,32,4};std::memcpy(table.data()+16,&row,sizeof row);
    std::memcpy(table.data()+32,"img",4);std::vector<DynamicTextureRecord> records;
    assert(ValidateDynamicTextureTable(table.data(),table.size(),2,records) && records.size()==1);
    for(size_t n=0;n<table.size();++n)assert(!ValidateDynamicTextureTable(table.data(),n,2,records) && records.empty());
    auto reject=[&](DynamicTextureRecord changed){auto bytes=table;std::memcpy(bytes.data()+16,&changed,sizeof changed);assert(!ValidateDynamicTextureTable(bytes.data(),bytes.size(),2,records) && records.empty());};
    auto changed=row;changed.texture=2;reject(changed);changed=row;changed.width=63;reject(changed);
    changed=row;changed.stride=63;reject(changed);changed=row;changed.nameOffset=31;reject(changed);
    changed=row;changed.nameOffset=0xffffffff;reject(changed);changed=row;changed.nameBytes=1;reject(changed);
    changed=row;changed.nameBytes=257;reject(changed);changed=row;changed.nameBytes=5;reject(changed);
    auto bytes=table;bytes.back()='x';assert(!ValidateDynamicTextureTable(bytes.data(),bytes.size(),2,records));
    bytes=table;bytes[33]=0;assert(!ValidateDynamicTextureTable(bytes.data(),bytes.size(),2,records));
    // Padding bytes in a source row are not image content. Opaque red at the
    // top-left and transparent blue at the bottom-right test flip and channels.
    std::array<uint8_t,8*10*2> source{};std::array<uint8_t,8*8*4> output{};
    source[0]=0xf8;source[1]=1;source[(7*10+7)*2]=0;source[(7*10+7)*2+1]=0x3e;
    assert(TileRgba5551(source.data(),source.size(),10,8,8,output.data(),output.size()));
    const unsigned top=42*4,bottom=21*4;
    assert(output[top]==255 && output[top+1]==0 && output[top+2]==0 && output[top+3]==255);
    assert(output[bottom]==0 && output[bottom+1]==255 && output[bottom+2]==0 && output[bottom+3]==0);
    assert(!TileRgba5551(nullptr,source.size(),10,8,8,output.data(),output.size()));
    assert(!TileRgba5551(source.data(),source.size()-1,10,8,8,output.data(),output.size()));
    assert(!TileRgba5551(source.data(),source.size(),7,8,8,output.data(),output.size()));
    assert(!TileRgba5551(source.data(),source.size(),10,7,8,output.data(),output.size()));
    assert(!TileRgba5551(source.data(),source.size(),10,8,8,output.data(),output.size()-1));
    std::array<uint8_t,512> overlap{};assert(!TileRgba5551(overlap.data(),128,8,8,8,overlap.data()+16,256));
    assert(!TileRgba5551(overlap.data()+16,128,8,8,8,overlap.data(),256));
    std::puts("PASS: mutable texture table bounds/names and tiled image stride, vertical orientation, channels, size and overlap contracts.");
}
