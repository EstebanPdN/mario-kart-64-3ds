#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace mk64_3ds {
// Count successful game-image submissions, including interpolation images and
// excluding skipped simulation ticks. All timing uses one monotonic clock.
class PresentationFps {
    struct Window {uint32_t frames=0;uint64_t ticks=0;};
    std::array<Window,10> history_{};
    uint64_t previous_=0,start_=0;
    uint32_t frames_=0;
    size_t next_=0,size_=0;
    bool started_=false;
    float instant_=0,oneSecond_=0,average_=0;
public:
    void Reset(){*this={};}
    bool Record(uint64_t now,uint64_t frequency) {
        if(!frequency)return false;
        if(!started_){started_=true;previous_=start_=now;return false;}
        if(now<=previous_) {
            if(now<previous_)Reset();
            return false;
        }
        instant_=static_cast<float>(frequency)/static_cast<float>(now-previous_);
        previous_=now;++frames_;
        const auto elapsed=now-start_;
        if(elapsed<frequency)return false;
        oneSecond_=static_cast<float>(frames_)*static_cast<float>(frequency)/static_cast<float>(elapsed);
        history_[next_]={frames_,elapsed};next_=(next_+1)%history_.size();
        if(size_<history_.size())++size_;
        uint64_t totalTicks=0,totalFrames=0;
        for(size_t i=0;i<size_;++i){totalTicks+=history_[i].ticks;totalFrames+=history_[i].frames;}
        average_=static_cast<float>(totalFrames)*static_cast<float>(frequency)/static_cast<float>(totalTicks);
        frames_=0;start_=now;return true;
    }
    float Instant()const{return instant_;}
    float OneSecond()const{return oneSecond_;}
    float Average()const{return average_;}
};
}
