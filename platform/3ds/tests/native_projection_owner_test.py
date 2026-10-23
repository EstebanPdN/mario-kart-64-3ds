#!/usr/bin/env python3
"""Check the real projection uploader against shared PICA uniform ownership."""
import argparse
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1]
source=(port/'source/gfx_citro3d.cpp').read_text()
start=source.index('void GfxRenderingAPICitro3D::UploadProjectionForActiveTarget(')
brace=source.index('{',start);end=brace+1;depth=1
while depth:
    depth+=(source[end]=='{')-(source[end]=='}');end+=1
body=source[start:end]
pre=r'''
#include "native_camera_3ds.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>
constexpr int GPU_VERTEX_SHADER=0;
struct FVec {float x,y,z,w;};
struct C3D_Mtx {FVec r[4];};
void Mtx_Identity(C3D_Mtx* m){*m={};m->r[0].x=m->r[1].y=m->r[2].z=m->r[3].w=1;}
void Mtx_Multiply(C3D_Mtx* m,const C3D_Mtx* a,const C3D_Mtx* b){
 const auto* x=reinterpret_cast<const float*>(a);const auto* y=reinterpret_cast<const float*>(b);auto* z=reinterpret_cast<float*>(m);
 for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){z[r*4+c]=0;for(unsigned k=0;k<4;++k)z[r*4+c]+=x[r*4+k]*y[k*4+c];}
}
namespace mk64_3ds {float ViewportSlideOffset(int x,int w){return float(x+w)/1000;}}
C3D_Mtx hardware;unsigned uploads=0;
void C3D_FVUnifMtx4x4(int,int,const C3D_Mtx* value){hardware=*value;++uploads;}
struct Impl {void* activeTarget=nullptr;void* sceneTarget=nullptr;int projectionUniform=0,viewportX=4,viewportY=8,viewportWidth=400,viewportHeight=240;};
class GfxRenderingAPICitro3D {
public:
 Impl state;Impl* mImpl=&state;void* mNativeSceneBatch=nullptr;void* mNativeSpriteBatch=nullptr;
 void UploadProjectionForActiveTarget(bool viewportSlide=true);
};
'''
post=r'''
int main(){
 using namespace mk64_3ds::native_scene;
 for(bool scene:{false,true})for(bool target:{false,true}) {
  GfxRenderingAPICitro3D renderer;int marker;
  renderer.state.sceneTarget=&marker;renderer.state.activeTarget=target?&marker:nullptr;
  renderer.UploadProjectionForActiveTarget();const auto fast=hardware;
  Camera camera{};camera.rows[0][0]=3;camera.rows[1][3]=7;
  CameraUniformCache cache;assert(cache.Update(camera));std::memcpy(&hardware,&camera,64);
  if(scene)renderer.mNativeSceneBatch=&marker;else renderer.mNativeSpriteBatch=&marker;
  const unsigned before=uploads;
  // SyncViewportAndDepth may call SetViewport before every native packet.
  renderer.UploadProjectionForActiveTarget();renderer.UploadProjectionForActiveTarget();
  assert(uploads==before && !std::memcmp(&hardware,&camera,64) && !cache.Update(camera));
  renderer.mNativeSceneBatch=renderer.mNativeSpriteBatch=nullptr;
  renderer.UploadProjectionForActiveTarget();assert(uploads==before+1 && !std::memcmp(&hardware,&fast,64));
  cache.Invalidate();assert(cache.Update(camera));
 }
 std::puts("PASS: actual Fast3D projection uploader respects scene/sprite MVP ownership, both target orientations and restoration after batch exit.");
}
'''
flags=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined',
       '-DMK64_3DS_NATIVE_LUIGI_LAB','-DMK64_3DS_NATIVE_KART_LAB',
       '-I',str(port/'include')]
for name,code in [('native-owner',body),('negative-control',body.replace('if(mNativeSceneBatch)return;','').replace('if(mNativeSpriteBatch)return;',''))]:
    cpp=a.output/(name+'.cpp');exe=a.output/name
    cpp.write_text(pre+code+post)
    subprocess.run(flags+[str(cpp),'-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    (a.output/(name+'.log')).write_text(result.stdout+result.stderr)
    assert (result.returncode==0)==(name=='native-owner'),result.stderr
print('PASS: projection owner regression and stale-register negative control.')
