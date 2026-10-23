#!/usr/bin/env python3
"""Exercise the actual canonical path lookup, including mutable-name collisions."""
import argparse, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1]
s=(port/'source/resource_runtime_3ds.cpp').read_text()
start=s.index('CrcEntry* FindCrcEntry(std::string_view path,');end=s.index('\n}',start)+2
body=s[start:end]
start=s.index('extern "C" uint64_t ResourceGetCrcByName(');end=s.index('\n}',start)+2
crcFunction=s[start:end]
assert s.count('sResourceNames.Reset();')==2 # Both archive initialization and shutdown.
test=r'''
#include "resource_name_cache_3ds.hpp"
#include <string>
#include <vector>
#include <memory>
#include <cassert>
#include <cstdio>
#include <cstring>
struct Archive {std::vector<std::string> names;const auto& Entries()const{return names;}};
struct CrcEntry {uint64_t crc;};
std::unique_ptr<Archive> sArchive;
std::vector<CrcEntry> sCrcEntries;
std::vector<uint32_t> sCrcSlots;
mk64_3ds::ResourceNameCache<> sResourceNames;
unsigned hashes=0;
uint64_t sResourceNameHits=0,sResourceNameMisses=0;
uint64_t PathCrc(std::string_view){++hashes;return 7;} // Deliberate full-hash collisions.
size_t CrcSlot(uint64_t value){return value&(sCrcSlots.size()-1);}
BODY
std::string_view NormalizePath(const char* name){
 if(!name)return {};std::string_view path(name);if(path.starts_with("__OTR__"))path.remove_prefix(7);return path;
}
CRC_FUNCTION
void Init(std::vector<std::string> names){
 sResourceNames.Reset();sArchive=std::make_unique<Archive>();sArchive->names=names;
 sCrcEntries.assign(names.size(),{7});sCrcSlots.assign(64,0);
 for(unsigned i=0;i<names.size();++i)sCrcSlots[(7+i)&63]=i+1;
}
int main(){
 Init({"textures/body","textures/wheel","textures/other"});
 char input[128]="textures/body";
 assert(FindCrcEntry(input)==&sCrcEntries[0]);const auto first=hashes;
 for(unsigned i=0;i<10000;++i)assert(FindCrcEntry(input)==&sCrcEntries[0]);
 assert(hashes==first);
 // Same incoming address, different contents must never return an old identity.
 std::strcpy(input,"textures/wheel");assert(FindCrcEntry(input)==&sCrcEntries[1]);
 std::strcpy(input,"textures/missing");assert(!FindCrcEntry(input));
 std::strcpy(input,"textures/body");assert(FindCrcEntry(input)==&sCrcEntries[0]);
 // An archive generation with reused entry indices still validates names.
 Init({"textures/changed","textures/body"});assert(FindCrcEntry(input)==&sCrcEntries[1]);
 const auto before=hashes;for(unsigned i=0;i<10000;++i){
  assert(FindCrcEntry(input)==&sCrcEntries[1]);assert(ResourceGetCrcByName(input)==7);
 }
 assert(ResourceGetCrcByName("__OTR__textures/body")==7);
 assert(hashes<=before+1);
 for(unsigned i=0;i<10000;++i){auto name=i%2?"textures/changed":"textures/body";
  std::strcpy(input,name);assert(FindCrcEntry(input)==&sCrcEntries[i%2?0:1]);}
 assert(!FindCrcEntry({}));
 const auto prior=hashes;assert(ResourceGetCrcByName("not/in/archive")==7 && hashes==prior+1);
 assert(ResourceGetCrcByName(nullptr)==0 && ResourceGetCrcByName("")==0);
 sArchive.reset();assert(!FindCrcEntry("textures/body"));assert(ResourceGetCrcByName("pre/init")==7);
 static_assert(sizeof(mk64_3ds::ResourceNameCache<>)==256);
 std::puts("PASS: actual path lookup skips 20000 repeated hashes; mutable buffer, full CRC collisions, archive resets and misses preserve exact identity with 256 fixed bytes.");
}
'''.replace('BODY',body).replace('CRC_FUNCTION',crcFunction)
cpp=a.output/'path.cpp';cpp.write_text(test);exe=a.output/'path'
subprocess.run(['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(port/'include'),str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
