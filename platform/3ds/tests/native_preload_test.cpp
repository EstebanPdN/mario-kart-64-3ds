#include "native_preload_3ds.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace mk64_3ds;
int main(int argc,char** argv){
 assert(NativePreloadBudget(16*1024*1024,8*1024*1024)==0);assert(NativePreloadBudget(16*1024*1024,16*1024*1024)==0);
 assert(NativePreloadBudget(16*1024*1024,7*1024*1024)==1024*1024);assert(NativePreloadBudget(16*1024*1024,0)==2*1024*1024);
 std::vector<uint8_t> bytes(24);std::memcpy(bytes.data(),"NPR1",4);uint32_t count=1;std::memcpy(bytes.data()+4,&count,4);NativeDependency d{42,1,64};std::memcpy(bytes.data()+8,&d,16);
 std::vector<NativeDependency> out;assert(ValidateNativeDependencies(bytes.data(),bytes.size(),out));
 assert(!ValidateNativeDependencies(bytes.data(),bytes.size()-1,out) && out.empty());
 d.kind=4;std::memcpy(bytes.data()+8,&d,16);assert(!ValidateNativeDependencies(bytes.data(),bytes.size(),out));
 for(int i=1;i<argc;++i){std::ifstream f(argv[i],std::ios::binary);assert(f);bytes={std::istreambuf_iterator<char>(f),{}};assert(ValidateNativeDependencies(bytes.data(),bytes.size(),out));assert(!out.empty());
  auto wrong=bytes;std::memcpy(wrong.data()+24,wrong.data()+8,8);assert(!ValidateNativeDependencies(wrong.data(),wrong.size(),out));}
}
