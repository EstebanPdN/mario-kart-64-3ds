#!/usr/bin/env python3
"""Check actual sprite geometry publication against immutable ordered indices."""
import argparse, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1];s=(port/'source/native_gpu_sprites_3ds.cpp').read_text()
end=s[s.index('void GpuSprites::EndBatch() {'):s.index('bool GpuSprites::Resolve(')]
geometry=s[s.index('    const unsigned vertexCount=style.quads*4,indexCount=style.quads*6;'):s.index('    if(!batchBound_) {')]
assert 'PrepareSpriteIndices(packets*2,indices_,packets*12)' in s
assert 'packetCapacity_=packets;deferGeometryCleaning_=true;' in s
prefix=r'''
#include "native_sprite_batch_3ds.hpp"
#include <array>
#include <vector>
#include <cassert>
#include <stdexcept>
#include <cstdio>
struct Vertex {short position[3];short uv[2];unsigned char color[4];};
std::vector<std::pair<const void*,size_t>> cleaned;
bool failClean=false;
bool Mk64System3DSCleanDataCache(const void* p,size_t n){if(failClean)return false;cleaned.push_back({p,n});return true;}
namespace mk64_3ds::native_scene {
struct GpuSprites {
 std::array<Vertex,64> storage{};std::array<uint16_t,96> indexStorage{};
 Vertex* vertices_=storage.data();uint16_t* indices_=indexStorage.data();
 unsigned usedVertices_=0,usedIndices_=0,flushedVertices_=0,flushedIndices_=0;
 bool deferGeometryCleaning_=false;
 struct {unsigned geometryCleanCalls=0,geometryCleanBytes=0;}statistics_;
 void EndBatch();
 void Geometry(const Vertex vertices[8],const SpriteStyle& style){GEOMETRY}
};
END
}
int main(){
 using namespace mk64_3ds::native_scene;
 std::array<uint16_t,16384*6> maximum{};
 assert(PrepareSpriteIndices(16384,maximum.data(),maximum.size()));
 for(unsigned q=0;q<16384;++q) {const unsigned pattern[]={0,1,2,0,2,3};
  for(unsigned i=0;i<6;++i)assert(maximum[q*6+i]==q*4+pattern[i]);}
 const auto saved=maximum;
 assert(!PrepareSpriteIndices(16385,maximum.data(),maximum.size()));
 assert(!PrepareSpriteIndices(8,maximum.data(),47));assert(saved==maximum);
 Vertex input[8]{};for(unsigned i=0;i<8;++i){input[i].position[0]=i;input[i].color[3]=255;}
 unsigned totals[2]{};
 for(bool deferred:{false,true}){
  GpuSprites gpu;gpu.deferGeometryCleaning_=deferred;
  assert(PrepareSpriteIndices(16,gpu.indices_,96));const auto initial=gpu.indexStorage;
  cleaned.clear();
  for(unsigned q:{1u,2u,1u,2u,2u}){
   SpriteStyle style;style.quads=q;const auto base=gpu.usedVertices_,offset=gpu.usedIndices_;
   gpu.Geometry(input,style);
   const unsigned pattern[]={0,1,2,0,2,3};
   for(unsigned j=0;j<q;++j)for(unsigned k=0;k<6;++k)
    assert(gpu.indices_[offset+j*6+k]==base+j*4+pattern[k]);
  }
  gpu.EndBatch();assert(gpu.indexStorage==initial);
  assert(cleaned.size()==(deferred?1:5));
  size_t bytes=0;for(auto [pointer,n]:cleaned){auto address=reinterpret_cast<uintptr_t>(pointer);
   assert(address>=reinterpret_cast<uintptr_t>(gpu.vertices_) && address+n<=reinterpret_cast<uintptr_t>(gpu.vertices_+64));bytes+=n;}
  assert(bytes==32*sizeof(Vertex));totals[deferred]=gpu.statistics_.geometryCleanCalls;
  if(deferred){gpu.EndBatch();assert(cleaned.size()==1);}
 }
 assert(totals[0]==5 && totals[1]==1);
 // A failed batch publication retains ownership and the exact dirty range.
 GpuSprites pending;pending.deferGeometryCleaning_=true;SpriteStyle style;
 pending.Geometry(input,style);failClean=true;bool rejected=false;
 try{pending.EndBatch();}catch(const std::runtime_error&){rejected=true;}
 assert(rejected && pending.flushedVertices_==0 && pending.flushedIndices_==0);
 failClean=false;cleaned.clear();pending.EndBatch();
 assert(cleaned.size()==1 && cleaned[0].first==pending.vertices_ && cleaned[0].second==style.quads*4*sizeof(Vertex));
 assert(pending.flushedVertices_==pending.usedVertices_ && pending.flushedIndices_==pending.usedIndices_);
 std::puts("PASS: 16384 quads retain exact ordered indices; mixed 1/2-quad actual geometry never mutates indices, publication cleans only written vertex bytes (5 immediate or 1 deferred calls).");
}
'''.replace('GEOMETRY',geometry).replace('END',end)
cpp=a.output/'publication.cpp';cpp.write_text(prefix);exe=a.output/'publication'
subprocess.run(['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(port/'include'),str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
