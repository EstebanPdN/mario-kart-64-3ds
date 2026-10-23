#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
namespace mk64_3ds {
// Development migration bridge only. The final native scene walks typed
// descriptors directly, without a legacy command-address anchoring layer.
template<class Packet,size_t Capacity=64,size_t Buckets=256> class NativeAnchorQueue {
    static_assert((Buckets&(Buckets-1))==0 && Buckets>=Capacity*2);
    struct Entry {uintptr_t anchor=0;Packet packet{};int next=-1;bool consumed=false;};
    std::array<Entry,Capacity> entries_{};std::array<int,Buckets> first_{},last_{};size_t count_=0;
    static size_t Bucket(uintptr_t a){return ((a>>3)^(a>>11))&(Buckets-1);}
public:
    NativeAnchorQueue(){Reset();}
    void Reset(){count_=0;first_.fill(-1);last_.fill(-1);}
    bool Push(uintptr_t anchor,const Packet& packet) {
        if(!anchor || count_==Capacity)return false;
        const unsigned index=count_++;entries_[index]={anchor,packet,-1,false};const size_t bucket=Bucket(anchor);
        if(first_[bucket]<0)first_[bucket]=index;else entries_[last_[bucket]].next=index;
        last_[bucket]=index;return true;
    }
    void BeginReplay(){for(size_t i=0;i<count_;++i)entries_[i].consumed=false;}
    template<class Draw> bool Consume(uintptr_t anchor,Draw draw) {
        bool matched=false;
        for(int i=first_[Bucket(anchor)];i>=0;i=entries_[i].next)if(entries_[i].anchor==anchor && !entries_[i].consumed) {
            draw(entries_[i].packet);entries_[i].consumed=true;matched=true;
        }
        return matched;
    }
    template<class Inspect> void InspectEntries(Inspect inspect)const {for(size_t i=0;i<count_;++i)inspect(entries_[i].anchor,entries_[i].consumed);}
    size_t Remaining()const {size_t n=0;for(size_t i=0;i<count_;++i)n+=!entries_[i].consumed;return n;}
    size_t Size()const{return count_;}
};
}
