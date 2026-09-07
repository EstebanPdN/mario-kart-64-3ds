#include "read_ahead_file_3ds.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <random>
#include <string>

int main(int argc, char** argv) {
    assert(argc == 2); // An isolated writable fixture path, never a game archive.
    std::vector<unsigned char> expected(100003);
    for (size_t i=0;i<expected.size();++i) expected[i] = (i*37 + i/17) & 255;
    FILE* file = std::fopen(argv[1], "wb"); assert(file);
    assert(std::fwrite(expected.data(),1,expected.size(),file)==expected.size());std::fclose(file);
    mk64_3ds::ReadAheadFile cached;
    assert(cached.Open(argv[1])); assert(cached.Size()==expected.size());
    std::vector<unsigned char> out(30000);
    assert(cached.Read(100, out.data(),30)==30);
    assert(cached.Read(220, out.data(),4200)==4200);
    assert(cached.ReadCalls()==1); // Header + name gap + payload: one underlying read.
    std::mt19937 rng(14);
    for(unsigned i=0;i<10000;++i) {
        size_t at=rng()%(expected.size()+20), n=rng()%out.size();
        size_t want=at>=expected.size()?0:std::min(n,expected.size()-at);
        assert(cached.Read(at,out.data(),n)==want);
        assert(std::equal(out.begin(),out.begin()+want,expected.begin()+std::min(at,expected.size())));
    }
    assert(cached.Read(UINT64_MAX,out.data(),50)==0);
    cached.Close();assert(cached.Read(0,out.data(),1)==0);
    assert(cached.Open(argv[1]));assert(cached.ReadCalls()==0);
    assert(cached.Read(100,out.data(),30)==30);assert(cached.ReadCalls()==1);
    std::remove(argv[1]);
    std::puts("read-ahead: header/payload coalescing, random boundaries, EOF, reopen passed");
}
