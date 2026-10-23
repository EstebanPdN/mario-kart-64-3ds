#pragma once
#include <cstdint>
#include <cstddef>
#include <limits>

namespace mk64_3ds {
// The diagnostic source bytes and typed packet describe the same contiguous
// producer emission. No opcode is inspected by this bounds/order policy.
struct NativeComparisonSpan {uintptr_t begin=0,end=0;};
inline bool ValidComparisonSpan(NativeComparisonSpan span) {
    return span.begin && span.end>span.begin && !(span.begin&7) && !(span.end&7) &&
        span.end-span.begin<=4096;
}
inline bool ComparisonSpansWithin(const NativeComparisonSpan* spans,size_t count,
        uintptr_t commands,size_t bytes) {
    if(!spans || !count || count>512 || !commands || (commands&7) || !bytes || (bytes&7) ||
       bytes>4*1024*1024 || bytes>std::numeric_limits<uintptr_t>::max()-commands)return false;
    const uintptr_t end=commands+bytes;uintptr_t previous=commands;
    for(size_t i=0;i<count;++i) {
        if(!ValidComparisonSpan(spans[i]) || spans[i].begin<previous || spans[i].end>end)return false;
        previous=spans[i].end;
    }
    return true;
}
}
