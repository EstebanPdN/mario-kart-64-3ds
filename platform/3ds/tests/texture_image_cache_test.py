#!/usr/bin/env python3
"""Test the actual generated texture lookup cache, including live policy changes."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1];patch=(port/'cmake/texture_image_cache_3ds.cmake').read_text()
start=patch.index('namespace {');end=patch.index('\nbool gfx_set_timg_handler_rdp',start);body=patch[start:end]
pre=r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
unsigned generation=1;int mode=0;unsigned loads=0;bool missing=false;
uint32_t Mk64Resource3DSGeneration(){return generation;}
int Mk64Settings3DSGetUiTextures(){return mode;}
struct Init {std::string Path;};
namespace Fast {struct Texture {
 std::shared_ptr<Init> init=std::make_shared<Init>();uint8_t pixel=0;uint8_t* ImageData=&pixel;
 std::shared_ptr<Init> GetInitData(){return init;}
};}
namespace Ship {
struct Archive {const char* HashToCString(uint64_t hash){return hash==1?"textures/one":hash==2?"textures/two":nullptr;}};
struct Manager {
 Archive archive;Archive* GetArchiveManager(){return &archive;}
 std::shared_ptr<Fast::Texture> LoadResourceProcess(const char* name){++loads;if(missing)return nullptr;
  auto t=std::make_shared<Fast::Texture>();t->init->Path=std::strncmp(name,"__OTR__",7)==0?name+7:name;t->pixel=mode;return t;}
};
struct Context {Manager manager;static Context* GetRawInstance(){static Context c;return &c;}Manager* GetResourceManager(){return &manager;}};
}
'''
post=r'''
int main(){
 assert(!Mk64TextureByName3DS(nullptr));assert(!Mk64TextureByName3DS(""));
 char buffer[64]="__OTR__textures/one";
 auto one=Mk64TextureByName3DS(buffer);assert(one && loads==1);
 for(int i=0;i<1000;++i)assert(Mk64TextureByName3DS(buffer)==one);
 assert(loads==1);
 std::strcpy(buffer,"__OTR__textures/two");
 auto two=Mk64TextureByName3DS(buffer);assert(two!=one && two->init->Path=="textures/two" && loads==2);
 // The previous texture remains alive: expiry alone cannot detect a mode change.
 mode=1;auto improved=Mk64TextureByName3DS(buffer);assert(improved!=two && improved->pixel==1 && loads==3);
 mode=0;auto original=Mk64TextureByName3DS(buffer);assert(original!=improved && original->pixel==0 && loads==4);
 ++generation;auto reopened=Mk64TextureByName3DS(buffer);assert(reopened!=original && loads==5);
 reopened.reset();auto expired=Mk64TextureByName3DS(buffer);assert(expired && loads==6);
 const char* name=nullptr;auto hash=Mk64TextureByHash3DS(1,&name);assert(hash && std::strcmp(name,"textures/one")==0);
 const unsigned before=loads;assert(Mk64TextureByHash3DS(1,&name)==hash && loads==before);
 mode=2;auto changed=Mk64TextureByHash3DS(1,&name);assert(changed!=hash && changed->pixel==2);
 assert(!Mk64TextureByHash3DS(9,&name) && name==nullptr);
 missing=true;std::strcpy(buffer,"textures/missing");assert(!Mk64TextureByName3DS(buffer));
 std::puts("PASS: production texture-image cache avoids 1000 repeated loads; preserves null/expired/missing resources, mutable names, archive generation and live Original/Improved/Menu switches.");
}
'''
flags=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined']
for name,code in [('cache',body),('negative-control',body.replace(' ||\n        entry.mode != Mk64TextureImageMode3DS()',''))]:
 cpp=a.output/(name+'.cpp');exe=a.output/name;cpp.write_text(pre+code+post)
 subprocess.run(flags+[str(cpp),'-o',str(exe)],check=True)
 result=subprocess.run([str(exe)],capture_output=True,text=True);(a.output/(name+'.log')).write_text(result.stdout+result.stderr)
 assert (result.returncode==0)==(name=='cache'),result.stdout+result.stderr
 if name=='cache':print(result.stdout,end='')
print('PASS: omitting texture-policy validation fails the negative control.')
