// Development-only paired measurement. The buffer is never submitted to a GPU.
#include "system_3ds.h"
#include <3ds.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <sys/stat.h>

extern "C" void Mk64NativeCacheProbe3DS() {
    auto release=[](uint8_t* p){linearFree(p);};
    std::unique_ptr<uint8_t,decltype(release)> buffer(
        static_cast<uint8_t*>(linearMemAlign(256,128)),release);
    if(!buffer)throw std::runtime_error("Native cache probe allocation");
    std::memset(buffer.get(),0,256);
    mkdir("sdmc:/3ds/MK64/native-lab",0777);
    FILE* file=std::fopen("sdmc:/3ds/MK64/native-lab/n13-cache.csv","wb");
    if(!file)throw std::runtime_error("Native cache probe output");
    std::fprintf(file,"bytes,path,samples,median_ticks,p95_ticks,min_ticks,max_ticks\n");
    constexpr unsigned samples=128;
    for(unsigned bytes:{12u,24u,64u,128u}) {
        std::array<uint64_t,samples> service{},direct{};
        for(unsigned i=0;i<samples+16;++i)for(unsigned pass=0;pass<2;++pass) {
            const bool native=(i+pass)%2;
            // Identical address/range and fresh CPU writes; alternate path order.
            buffer.get()[(i*17)%bytes]^=static_cast<uint8_t>(i+1);
            const uint64_t start=svcGetSystemTick();
            const bool ok=native?Mk64System3DSCleanDataCache(buffer.get(),bytes):
                R_SUCCEEDED(GSPGPU_FlushDataCache(buffer.get(),bytes));
            const uint64_t ticks=svcGetSystemTick()-start;
            if(!ok){std::fclose(file);throw std::runtime_error("Native cache probe transfer");}
            if(i>=16)(native?direct:service)[i-16]=ticks;
        }
        for(bool native:{false,true}) {
            auto& times=native?direct:service;std::sort(times.begin(),times.end());
            std::fprintf(file,"%u,%s,%u,%llu,%llu,%llu,%llu\n",bytes,
                native?"platform_clean":"gsp_service",samples,
                (unsigned long long)times[samples/2],
                (unsigned long long)times[samples*95/100],
                (unsigned long long)times.front(),(unsigned long long)times.back());
        }
    }
    std::fprintf(file,"cache_mode,%s\nticks_per_second,%llu\n",Mk64System3DSDataCacheMode(),
        (unsigned long long)Mk64System3DSTicksPerSecond());
    std::fclose(file);
}
