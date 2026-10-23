#include "palette_copy_3ds.hpp"
#include <array>
#include <memory>
#include <cassert>
int main() {
    auto source=std::make_unique<uint8_t[]>(464);
    for(unsigned i=0;i<464;++i)source[i]=i;
    std::array<uint8_t,514> output;output.fill(0xc3);
    assert(mk64_3ds::CopyPaletteBytes(output.data()+1,source.get(),256,source.get(),464));
    assert(mk64_3ds::CopyPaletteBytes(output.data()+257,source.get()+256,256,source.get(),464));
    for(unsigned i=0;i<464;++i)assert(output[i+1]==uint8_t(i));
    for(unsigned i=464;i<512;++i)assert(output[i+1]==0);
    assert(output.front()==0xc3 && output.back()==0xc3);
    const auto valid=output;
    const auto* outside=reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(source.get())+465);
    assert(!mk64_3ds::CopyPaletteBytes(output.data(),outside,16,source.get(),464));
    assert(!mk64_3ds::CopyPaletteBytes(output.data(),source.get(),513,nullptr,0));
    assert(output==valid);
    std::array<uint8_t,512> raw{};raw.back()=0x7f;
    assert(mk64_3ds::CopyPaletteBytes(output.data()+1,raw.data(),512,nullptr,0));
    assert(output[512]==0x7f);
}
