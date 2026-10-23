#include "native_comparison_span_3ds.hpp"
#include <array>
#include <cassert>
using namespace mk64_3ds;
int main() {
    std::array<NativeComparisonSpan,3> spans{{{0x1000,0x1030},{0x1030,0x1080},{0x1100,0x1180}}};
    assert(ComparisonSpansWithin(spans.data(),3,0x1000,0x180));
    for(unsigned i=0;i<3;++i) {
        const auto saved=spans[i];spans[i].end=spans[i].begin;
        assert(!ComparisonSpansWithin(spans.data(),3,0x1000,0x180));spans[i]=saved;
        spans[i].begin+=1;assert(!ComparisonSpansWithin(spans.data(),3,0x1000,0x180));spans[i]=saved;
        spans[i].end+=1;assert(!ComparisonSpansWithin(spans.data(),3,0x1000,0x180));spans[i]=saved;
    }
    auto reversed=spans;reversed[1]=spans[0];reversed[0]=spans[1];
    assert(!ComparisonSpansWithin(reversed.data(),3,0x1000,0x180));
    auto overlap=spans;overlap[1].begin-=8;
    assert(!ComparisonSpansWithin(overlap.data(),3,0x1000,0x180));
    assert(!ComparisonSpansWithin(spans.data(),3,0x1000,0x178));
    assert(!ComparisonSpansWithin(spans.data(),3,0x1008,0x180));
    assert(!ComparisonSpansWithin(spans.data(),0,0x1000,0x180));
    assert(!ComparisonSpansWithin(nullptr,3,0x1000,0x180));
    assert(!ComparisonSpansWithin(spans.data(),513,0x1000,0x180));
    assert(!ComparisonSpansWithin(spans.data(),3,0x1001,0x180));
    assert(!ComparisonSpansWithin(spans.data(),3,0x1000,4*1024*1024+8));
    assert(!ComparisonSpansWithin(spans.data(),3,std::numeric_limits<uintptr_t>::max()-7,16));
    assert(!ValidComparisonSpan({0x1000,0x2008}));
    assert(ValidComparisonSpan({0x1000,0x2000}));
}
