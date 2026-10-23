#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace mk64_3ds {
// A frame owns decoded native arguments and direct ARM callbacks. Source-word
// indices are construction tokens only: replay never reads graphics words or
// chooses an opcode handler. Callbacks consume their argument view immediately;
// GPU-visible storage must have its own completion-owned lifetime.
template<size_t WordSlots,size_t CommandSlots,size_t ArgumentWords>
class NativeFrameQueue {
    static_assert(WordSlots>0 && CommandSlots>0 && CommandSlots<65535 && ArgumentWords>0);
public:
    using Callback=void(*)(void*,const uint32_t*,size_t);
    static constexpr uint16_t kNone=65535;
    bool Begin(size_t words) {
        if(replaying_ || !words || words>WordSlots)return false;
        if(++generation_==0){for(auto& slot:slots_)slot={};++generation_;}
        words_=words;commands_=arguments_=0;sealed_=false;failed_=false;missing_=0;return true;
    }
    // Zero-span draws can share an anchor with a later state/flow command.
    // One final positive span closes that anchor. Reject multiple writers and
    // overlap instead of silently falling back to the old command decoder.
    bool Append(size_t word,uint16_t span,Callback callback,const uint32_t* args,size_t count) {
        if(replaying_ || sealed_ || failed_)return false;
        if(!callback || word>=words_ || span>words_-word ||
           count>32 || (count && !args) || commands_==CommandSlots || count>ArgumentWords-arguments_)return Fail();
        auto& slot=slots_[word];
        if(slot.generation!=generation_)slot={generation_,kNone,kNone};
        if(slot.tail!=kNone && nodes_[slot.tail].span)return Fail();
        const auto index=static_cast<uint16_t>(commands_++);
        auto& node=nodes_[index];node={callback,static_cast<uint32_t>(arguments_),kNone,span,static_cast<uint16_t>(count)};
        if(count)std::memcpy(argumentsData_.data()+arguments_,args,count*sizeof(uint32_t));arguments_+=count;
        if(slot.tail==kNone)slot.head=index;else nodes_[slot.tail].next=index;
        slot.tail=index;return true;
    }
    bool Seal() {
        if(replaying_ || failed_ || sealed_ || !words_)return false;
        for(size_t word=0;word<words_;) {
            const auto& slot=slots_[word];
            if(slot.generation!=generation_ || slot.tail==kNone || !nodes_[slot.tail].span){missing_=word;return Fail();}
            const size_t span=nodes_[slot.tail].span;
            for(size_t i=1;i<span;++i)if(slots_[word+i].generation==generation_){missing_=word+i;return Fail();}
            word+=span;
        }
        sealed_=true;return true;
    }
    bool Replay(void* context) {
        if(replaying_ || !sealed_ || failed_)return false;
        replaying_=true;
        // If a callback raises, keep the frame immutable but permit orderly
        // owner cleanup; never leave the buffer permanently marked executing.
        struct Guard {bool& value;~Guard(){value=false;}} guard{replaying_};
        try {
        for(size_t word=0;word<words_;) {
            const auto& slot=slots_[word];uint16_t at=slot.head;
            while(at!=kNone) {
                const auto& node=nodes_[at];node.callback(context,argumentsData_.data()+node.arguments,node.count);at=node.next;
            }
            word+=nodes_[slot.tail].span;
        }
        } catch(...) {failed_=true;throw;}
        return true;
    }
    size_t MissingWord()const{return missing_;}
    size_t Commands()const{return commands_;}
    size_t Arguments()const{return arguments_;}
    bool Failed()const{return failed_;}
private:
    struct Slot {uint32_t generation=0;uint16_t head=kNone,tail=kNone;};
    struct Node {Callback callback=nullptr;uint32_t arguments=0;uint16_t next=kNone,span=0,count=0;};
    bool Fail(){failed_=true;return false;}
    std::array<Slot,WordSlots> slots_{};
    std::array<Node,CommandSlots> nodes_{};
    std::array<uint32_t,ArgumentWords> argumentsData_{};
    uint32_t generation_=0;
    size_t words_=0,commands_=0,arguments_=0,missing_=0;
    bool sealed_=false,failed_=false,replaying_=false;
};
}
