#include "texture_lease_3ds.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace {
struct Entry { std::array<uint8_t,4> bytes{1,2,3,4}; unsigned owners=0; bool live=true; };
std::array<Entry,2> entries;
void ReclaimUnowned() { for(auto& e:entries)if(!e.owners)e.live=false; }
}
extern "C" Mk64ResourceLoadResult3DS Mk64Resource3DSAcquireTexture(const char* name,Mk64TextureResource3DS* out) {
    *out={};
    if(!name || !*name || !std::strcmp(name,"missing"))return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
    ReclaimUnowned();
    const size_t i=!std::strcmp(name,"palette");auto& e=entries[i];e.live=true;++e.owners;
    out->data=e.bytes.data();out->size=e.bytes.size();out->lifetimeToken=i+1;
    return MK64_RESOURCE_LOAD_OK_3DS;
}
extern "C" void Mk64Resource3DSReleaseTexture(uint64_t token) {
    assert(token && token<=entries.size());auto& e=entries[token-1];assert(e.owners);--e.owners;
}
int main() {
    using mk64_3ds::TextureLease3DS;
    static_assert(!std::is_copy_constructible_v<TextureLease3DS>);
    static_assert(!std::is_copy_assignable_v<TextureLease3DS>);
    {
        TextureLease3DS image("image"),palette("palette");
        ReclaimUnowned();
        assert(image.Valid() && palette.Valid() && entries[0].live && entries[1].live);
        assert(image.Get().data[0]==1 && palette.Get().data[3]==4);
        palette.Reset();ReclaimUnowned();assert(entries[0].live && !entries[1].live);
        assert(!palette.Acquire("missing") && !palette.Valid());
    }
    assert(!entries[0].owners && !entries[1].owners);
    try {TextureLease3DS image("image");throw std::runtime_error("failed upload");}catch(const std::runtime_error&) {}
    assert(!entries[0].owners);ReclaimUnowned();assert(!entries[0].live);
    std::puts("PASS: texture/palette survive intervening cache reclamation; failure, early reset and exception release their leases.");
}
