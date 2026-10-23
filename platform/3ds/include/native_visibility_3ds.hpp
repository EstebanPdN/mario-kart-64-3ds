#pragma once

#include "native_camera_3ds.hpp"
#include "native_scene_format_3ds.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace mk64_3ds::native_scene {

struct VisibilityStatistics {
    std::uint64_t rangeTests=0, rangeReuses=0, rejectedRanges=0;
    std::uint64_t rejectedTriangles=0, blockTests=0, extraDraws=0;
    std::uint64_t matrixUploads=0, matrixReuses=0;
};
// The renderer is the only writer; diagnostics read while gameplay is paused.
inline VisibilityStatistics gNativeVisibilityStatistics;

struct GeometryBounds {
    std::int16_t minimum[3]{}, maximum[3]{};
};
static_assert(sizeof(GeometryBounds)==12);

// Planes use the exact native shader's object-to-PICA matrix. Keep numerical
// uncertainty visible, including float24 uniform/dot-product rounding and
// cancellation between the depth and W rows. No distance/quality reduction.
class NativeClipFrustum {
public:
    explicit NativeClipFrustum(const Camera& camera) {
        for(unsigned plane=0;plane<6;++plane) {
            const unsigned row=plane<4?plane/2:2;
            const float sign=plane==5?-1.f:(plane&1)?-1.f:1.f;
            for(unsigned c=0;c<4;++c) {
                const float w=plane==5?0.f:camera.rows[3][c];
                const float v=camera.rows[row][c];
                planes_[plane][c]=w+sign*v;
                magnitudes_[plane][c]=std::fabs(w)+std::fabs(v);
                finite_ &= std::isfinite(planes_[plane][c]) &&
                           std::isfinite(magnitudes_[plane][c]);
            }
        }
    }

    enum Relation { Outside, Intersecting, Inside };
    Relation Classify(const GeometryBounds& bounds) const {
        if(!finite_)return Intersecting;
        bool inside=true;
        for(unsigned plane=0;plane<6;++plane) {
            const auto& p=planes_[plane];
            float maximum=p[3],minimum=p[3],magnitude=magnitudes_[plane][3];
            for(unsigned c=0;c<3;++c) {
                const float lo=bounds.minimum[c],hi=bounds.maximum[c];
                maximum+=p[c]*(p[c]>=0?hi:lo);
                minimum+=p[c]*(p[c]>=0?lo:hi);
                magnitude+=magnitudes_[plane][c]*std::max(std::fabs(lo),std::fabs(hi));
            }
            // A float24 has 16 fraction bits. This wider guard also covers
            // four products/additions, uniform conversion and plane creation.
            const float slack=magnitude*(1.f/1024.f)+1.f/1024.f;
            if(maximum < -slack)return Outside;
            if(!(minimum >= slack))inside=false;
        }
        return inside?Inside:Intersecting;
    }
private:
    float planes_[6][4]{},magnitudes_[6][4]{};
    bool finite_=true;
};

struct VisibleIndexSpan { std::uint32_t first=0,count=0; };
struct NativeVisibleRanges {
    static constexpr unsigned Capacity=4;
    std::array<VisibleIndexSpan,Capacity> spans{};
    unsigned count=0;
    void Append(VisibleIndexSpan span) {
        if(!span.count)return;
        if(count && spans[count-1].first+spans[count-1].count==span.first) {
            spans[count-1].count+=span.count;return;
        }
        if(count<Capacity){spans[count++]=span;return;}
        // Merge the smallest invisible gap, including the new tail. This may
        // draw extra invisible triangles; it never removes/reorders a visible one.
        unsigned best=Capacity-1;
        std::uint32_t gap=span.first-spans.back().first-spans.back().count;
        for(unsigned i=0;i+1<Capacity;++i) {
            const auto candidate=spans[i+1].first-spans[i].first-spans[i].count;
            if(candidate<gap){gap=candidate;best=i;}
        }
        if(best==Capacity-1) {
            spans.back().count=span.first+span.count-spans.back().first;return;
        }
        spans[best].count=spans[best+1].first+spans[best+1].count-spans[best].first;
        for(unsigned i=best+1;i+1<Capacity;++i)spans[i]=spans[i+1];
        spans.back()=span;
    }
};

// Built once from validated immutable native geometry. Small material ranges
// need just one compact box; only larger ranges allocate 128-triangle blocks.
// All per-frame work uses existing storage. Allocation/capacity failure keeps
// the correct full-range submission path.
class NativeVisibilityPlan {
    struct Range {
        GeometryBounds bounds;
        std::uint32_t firstBlock=0,blockCount=0;
    };
    struct Block { GeometryBounds bounds;VisibleIndexSpan indices; };
    static constexpr std::uint32_t BlockIndices=128*3;
    static constexpr std::size_t MaximumBytes=128*1024;
public:
    void Reset() {
        std::vector<Range>().swap(ranges_);std::vector<Block>().swap(blocks_);
        std::vector<std::uint8_t>().swap(classification_);cameraValid_=false;
    }
    bool Build(const Vertex* vertices,std::uint32_t vertexCount,
               const std::uint16_t* indices,std::uint32_t indexCount,
               const DrawRange* ranges,std::size_t count) {
        Reset();
        if(!vertices || !vertexCount || !indices || !ranges || !count ||
           count>MaximumBytes/(sizeof(Range)+1))return false;
        std::size_t blocks=0;
        for(std::size_t i=0;i<count;++i) {
            const auto& range=ranges[i];
            if(!range.count || range.count%3 || range.first%3 ||
               range.first>indexCount || range.count>indexCount-range.first)return false;
            if(range.count>BlockIndices)blocks+=(range.count-1)/BlockIndices+1;
            if(blocks>(MaximumBytes-count*(sizeof(Range)+1))/sizeof(Block))return false;
            for(std::uint32_t j=range.first;j<range.first+range.count;++j)
                if(indices[j]>=vertexCount)return false;
        }
        try {
            ranges_.resize(count);classification_.assign(count,0);blocks_.reserve(blocks);
            for(std::size_t i=0;i<count;++i) {
                const auto& input=ranges[i];auto& range=ranges_[i];
                range.bounds=Bounds(vertices,indices,input.first,input.count);
                if(input.count<=BlockIndices)continue;
                range.firstBlock=blocks_.size();
                for(std::uint32_t at=input.first;at<input.first+input.count;at+=BlockIndices) {
                    const auto length=std::min(BlockIndices,input.first+input.count-at);
                    blocks_.push_back({Bounds(vertices,indices,at,length),{at,length}});
                }
                range.blockCount=blocks_.size()-range.firstBlock;
            }
        }catch(const std::bad_alloc&){Reset();return false;}
        return true;
    }
    bool Ready()const{return !ranges_.empty();}
    std::size_t Bytes()const {
        return ranges_.capacity()*sizeof(Range)+blocks_.capacity()*sizeof(Block)+classification_.capacity();
    }
    void SetCamera(const Camera& camera) {
        if(cameraValid_ && std::memcmp(&camera_,&camera,sizeof camera)==0)return;
        camera_=camera;cameraValid_=true;frustum_=NativeClipFrustum(camera);
        std::fill(classification_.begin(),classification_.end(),0);
    }
    NativeVisibleRanges Select(std::size_t id,const DrawRange& input) {
        NativeVisibleRanges output;
        if(!cameraValid_ || id>=ranges_.size()){output.Append({input.first,input.count});return output;}
        const auto& range=ranges_[id];auto& cached=classification_[id];
        if(!cached) {
            ++gNativeVisibilityStatistics.rangeTests;
            cached=1+frustum_.Classify(range.bounds);
        }else ++gNativeVisibilityStatistics.rangeReuses;
        if(cached==1){
            ++gNativeVisibilityStatistics.rejectedRanges;
            gNativeVisibilityStatistics.rejectedTriangles+=input.count/3;return output;
        }
        if(cached==3 || !range.blockCount){output.Append({input.first,input.count});return output;}
        for(std::uint32_t i=range.firstBlock;i<range.firstBlock+range.blockCount;++i) {
            ++gNativeVisibilityStatistics.blockTests;
            const auto& block=blocks_[i];
            if(frustum_.Classify(block.bounds)!=NativeClipFrustum::Outside)output.Append(block.indices);
        }
        std::uint32_t submitted=0;
        for(unsigned i=0;i<output.count;++i)submitted+=output.spans[i].count;
        gNativeVisibilityStatistics.rejectedTriangles+=(input.count-submitted)/3;
        if(!output.count)++gNativeVisibilityStatistics.rejectedRanges;
        if(output.count>1)gNativeVisibilityStatistics.extraDraws+=output.count-1;
        return output;
    }
private:
    static GeometryBounds Bounds(const Vertex* vertices,const std::uint16_t* indices,
                                 std::uint32_t first,std::uint32_t count) {
        GeometryBounds bounds;
        for(unsigned c=0;c<3;++c)bounds.minimum[c]=bounds.maximum[c]=vertices[indices[first]].position[c];
        for(std::uint32_t i=first+1;i<first+count;++i)for(unsigned c=0;c<3;++c) {
            const auto value=vertices[indices[i]].position[c];
            bounds.minimum[c]=std::min(bounds.minimum[c],value);
            bounds.maximum[c]=std::max(bounds.maximum[c],value);
        }
        return bounds;
    }
    std::vector<Range> ranges_;std::vector<Block> blocks_;
    std::vector<std::uint8_t> classification_;
    Camera camera_{};NativeClipFrustum frustum_{Camera{}};bool cameraValid_=false;
};

} // namespace mk64_3ds::native_scene
