#!/usr/bin/env python3
"""Exercise the actual native procedure geometry functions without game assets."""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
text=(Path(__file__).resolve().parents[1]/'source/native_program_3ds.cpp').read_text()
def function(marker):
 start=text.index(marker);begin=text.index('{',start);i=begin+1;depth=1
 while depth:depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]
code=r'''
#include <array>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>
namespace np {struct Primitive{uint8_t a,b,c,d;};}
struct Mutation{bool mirror=false;};std::array<Mutation,4> mutations;
uint64_t triangles=0;int active=0;unsigned generations=0;
void Mk64NativeProgramTriangleBoundary(int state){if(state==0){++generations;active=0;}else active=state>0;}
struct Backend{std::vector<std::array<unsigned,3>> draws;void GfxSpTri1(unsigned a,unsigned b,unsigned c,bool rectangle){assert(active && !rectangle);draws.push_back({a,b,c});}};
struct Context{Backend* g;
'''
code+='\n'.join(function(marker) for marker in ('[[gnu::noinline]] void Triangle(', '[[gnu::noinline]] void Primitives(', '[[gnu::noinline]] void Quad('))
code+=r'''
};
int main(){Backend backend;Context c{&backend};std::vector<std::array<unsigned,3>> expected;
 for(unsigned mirror=0;mirror<2;++mirror){mutations[1].mirror=mirror;
  for(unsigned seed=0;seed<65536;++seed){std::array<np::Primitive,4> primitives{};
   for(unsigned i=0;i<4;++i)primitives[i]={uint8_t((seed+i)&63),uint8_t((seed/3+i*7)&63),uint8_t((seed/7+i*3)&63),uint8_t(i&1?((seed/11+i)&63):255)};
   backend.draws.clear();expected.clear();const auto oldGeneration=generations;
   for(auto p:primitives){if(mirror)std::swap(p.b,p.c);expected.push_back({p.a,p.b,p.c});if(p.d!=255)expected.push_back({p.a,p.c,p.d});}
   c.Primitives(1,primitives.data(),primitives.size());assert(backend.draws==expected && !active && generations==oldGeneration);
   backend.draws.clear();for(auto p:primitives){if(p.d==255)c.Triangle(1,p.a,p.b,p.c);else c.Quad(1,p.a,p.b,p.c,p.d);}
   assert(backend.draws==expected && !active && generations==oldGeneration);
  }
 }
 assert(triangles==uint64_t(2)*65536*6*2);
}
'''
src=a.output/'native-program-geometry.cpp';src.write_text(code);exe=a.output/'native-program-geometry'
subprocess.run(['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('PASS: actual native Triangle/Quad/Primitives functions, 131072 mirrored/normal batches, exact indices/count/order, pure triangle generations and inactive exits.')
