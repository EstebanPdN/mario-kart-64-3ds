#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>

namespace mk64_3ds::native_scene {
// Immutable original wheel TLUT bytes, prepared offline from the owner's archive.
// The dense canonical name mapping avoids allocations, hashing and ZIP/resource
// decoding in load_wheel_palette_non_blocking. Failed preparation retains the
// ordinary resource path; no partially validated pool is ever published.
class WheelPalettes final {
public:
    static constexpr size_t kBytes = 8 * 289 * 4 * 128;
    bool Load(const char* path) {
        if (bytes_) return true;
        using File = std::unique_ptr<FILE, int(*)(FILE*)>;
        File file(std::fopen(path,"rb"),std::fclose);
        if (!file) return false;
        std::array<uint8_t,32> h{};
        if (std::fread(h.data(),1,h.size(),file.get())!=h.size() ||
            std::memcmp(h.data(),"NWP1",4) || U32(h.data()+4)!=1 ||
            U32(h.data()+8)!=8 || U32(h.data()+12)!=289 ||
            U32(h.data()+16)!=4 || U32(h.data()+20)!=128 ||
            U32(h.data()+24)!=kBytes+32) return false;
        std::unique_ptr<uint8_t[]> candidate(new(std::nothrow) uint8_t[kBytes]);
        if (!candidate || std::fread(candidate.get(),1,kBytes,file.get())!=kBytes ||
            std::fgetc(file.get())!=EOF || std::ferror(file.get())) return false;
        uint32_t crc=~0u;
        for(size_t i=0;i<kBytes;++i) {
            crc^=candidate[i];
            for(unsigned b=0;b<8;++b) crc=(crc>>1)^((crc&1)?0xedb88320u:0);
        }
        if (~crc!=U32(h.data()+28)) return false;
        bytes_=std::move(candidate); return true;
    }
    const uint8_t* Find(const char* name) const {
        if (!bytes_ || !name) return nullptr;
        std::string_view path(name);
        if(path.substr(0,7)=="__OTR__") path.remove_prefix(7);
        constexpr std::string_view root="textures/karts/";
        if(path.substr(0,root.size())!=root) return nullptr;
        path.remove_prefix(root.size());
        constexpr std::array<std::string_view,8> characters={
            "mario", "luigi", "yoshi", "toad", "donkeykong", "wario", "peach", "bowser"};
        for(size_t c=0;c<characters.size();++c) {
            auto s=path; const auto n=characters[c];
            if(s.substr(0,n.size())!=n) continue;
            s.remove_prefix(n.size());
            if(s.substr(0,6)!="_kart/") continue;
            s.remove_prefix(6);
            if(s.substr(0,n.size())!=n) continue;
            s.remove_prefix(n.size());
            if(s.substr(0,6)!="_kart_") continue;
            s.remove_prefix(6);
            if(s.size()!=16 || s.substr(3,12)!="_tlut_wheel_") continue;
            unsigned frame=0;
            for(unsigned i=0;i<3;++i) {
                if(s[i]<'0'||s[i]>'9') return nullptr;
                frame=frame*10+unsigned(s[i]-'0');
            }
            if(frame>=289 || s[15]<'0'||s[15]>'3') return nullptr;
            return bytes_.get()+((c*289+frame)*4+unsigned(s[15]-'0'))*128;
        }
        return nullptr;
    }
    size_t ResidentBytes() const { return bytes_ ? kBytes : 0; }
private:
    static uint32_t U32(const uint8_t* p) {
        return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
    }
    std::unique_ptr<uint8_t[]> bytes_;
};
}
