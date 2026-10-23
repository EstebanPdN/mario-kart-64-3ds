#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <string_view>

namespace mk64_3ds {
// The address chooses a slot, never establishes identity. Compare the incoming
// bytes against the current archive's canonical name on every hit. Retain only
// an index, not a pointer into a caller's temporary/mutable string.
template<size_t Capacity=64> class ResourceNameCache {
    static_assert(Capacity && !(Capacity&(Capacity-1)));
    std::array<uint32_t,Capacity> entries_{};
    static size_t Slot(std::string_view path) {
        const auto address=reinterpret_cast<uintptr_t>(path.data());
        return ((address>>3)^(address>>11))&(Capacity-1);
    }
public:
    void Reset(){entries_.fill(0);}
    template<class NameAt> uint32_t Find(std::string_view path,NameAt nameAt)const {
        if(path.empty())return 0;
        const auto index=entries_[Slot(path)];
        return index && nameAt(index-1)==path ? index : 0;
    }
    void Remember(std::string_view path,uint32_t encodedIndex) {
        if(!path.empty())entries_[Slot(path)]=encodedIndex;
    }
};
}
