#include "native_projectile_contract_3ds.h"
#include <cassert>
#include <cstring>
int main(){
    for(unsigned p=0;p<65536;++p){
        // Independent N64 source operation: s16 fields, big-endian word.
        const int16_t red=static_cast<int16_t>(p&0xf800);
        const int16_t green=static_cast<int16_t>(p&0x7c0);
        const auto expected=uint16_t((red>>5)|(green<<5)|(p&0x3f));
        assert(Mk64NativeRedShellColor(p)==expected);
    }
    uint8_t source[514]{},dest[514]{};source[0]=source[513]=dest[0]=dest[513]=0xa5;
    for(unsigned i=0;i<256;++i){source[1+2*i]=i;source[2+2*i]=255-i;}
    Mk64NativeBuildRedShellPalette(source+1,dest+1);
    assert(source[0]==0xa5 && source[513]==0xa5 && dest[0]==0xa5 && dest[513]==0xa5);
    for(unsigned i=0;i<256;++i){unsigned value=(source[1+2*i]<<8)|source[2+2*i];assert(((dest[1+2*i]<<8)|dest[2+2*i])==Mk64NativeRedShellColor(value));}
    uint8_t inplace[512];std::memcpy(inplace,source+1,512);Mk64NativeBuildRedShellPalette(inplace,inplace);assert(!std::memcmp(inplace,dest+1,512));
}
