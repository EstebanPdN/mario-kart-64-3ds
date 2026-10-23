#include "native_batch_comparison_3ds.hpp"
#include <array>
#include <cassert>
#include <limits>
using namespace mk64_3ds::native_scene;
int main() {
    std::array<uint8_t,20> bytes{};
    auto write=[&](size_t at,uint32_t value){for(unsigned i=0;i<4;++i)bytes[at+i]=value>>(i*8);};
    std::memcpy(bytes.data(),"NBP1",4);write(4,1);write(8,123);write(16,246);
    BatchComparison result;
    assert(ValidateBatchComparison(bytes.data(),bytes.size(),123,492,result));
    assert(result.roots==123 && result.referenceBase==0 && result.optimizedBase==246);
    for(size_t n=0;n<20;++n){assert(!ValidateBatchComparison(bytes.data(),n,123,492,result));assert(!result.roots);}
    for(size_t at:{size_t(0),size_t(4),size_t(8),size_t(12),size_t(16)}) {
        auto saved=bytes;write(at,std::numeric_limits<uint32_t>::max());
        assert(!ValidateBatchComparison(bytes.data(),bytes.size(),123,492,result));assert(!result.roots);
        bytes=saved;
    }
    // Aliasing, incomplete view sets and root-count arithmetic are rejected.
    write(16,0);assert(!ValidateBatchComparison(bytes.data(),20,123,492,result));write(16,246);
    for(uint32_t roots:{0u,122u,124u,1025u,0x4000007bu})
        assert(!ValidateBatchComparison(bytes.data(),20,roots,492,result));
    assert(!ValidateBatchComparison(bytes.data(),20,123,491,result));
    assert(!ValidateBatchComparison(nullptr,20,123,492,result));
    std::array<uint8_t,21> unaligned{};std::memcpy(unaligned.data()+1,bytes.data(),20);
    assert(ValidateBatchComparison(unaligned.data()+1,20,123,492,result));
}
