#!/usr/bin/env python3
"""Exercise production portrait geometry with captured Citro2D calls, without assets."""
import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
port = Path(__file__).resolve().parents[1]
source = (port / 'source/bottom_ui_3ds.cpp').read_text()
def function(name):
    start = re.search(r'^void ' + name + r'\(', source, re.M).start()
    body = source.index('{', start)
    depth = 0
    for pos in range(body, len(source)):
        if source[pos] == '{': depth += 1
        elif source[pos] == '}':
            depth -= 1
            if not depth: return source[start:pos+1]
    raise AssertionError(name)
# This connection is essential: testing only the backing helper missed the old
# independently drawn stationary rectangle in the production standings path.
standings = function('DrawStandings')
assert 'DrawStandingBacking(portrait, box, angle, portraitAlpha);' in standings
assert 'C2D_DrawRectSolid' not in standings
header = r'''
#include <array>
#include <vector>
#include <cmath>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include "hud_layout_3ds.hpp"
struct Native { int texture, subTexture; };
struct UiTexture { bool initialized=true; unsigned width=32,height=32; bool native=false; };
Native nativeTexture{};
Native* NativeVariantFor(UiTexture& t,float,float) {return t.native?&nativeTexture:nullptr;}
struct C2D_Image {int* tex;int* subtex;};
C2D_Image TextureImage(UiTexture&) {return {nullptr,nullptr};}
struct C2D_ImageTint {};
struct C2D_DrawParams {struct {float x,y,w,h;} pos;struct {float x,y;} center;float depth,angle;};
C2D_DrawParams image{};
void DrawImageWithOpacity(C2D_Image,const C2D_DrawParams* p,const C2D_ImageTint*) {image=*p;}
struct Point {float x,y;};
std::vector<Point> corners;
uint32_t colorSeen;
float depthSeen;
uint32_t C2D_Color32(uint8_t r,uint8_t g,uint8_t b,uint8_t a) {return r|(g<<8)|(b<<16)|(uint32_t(a)<<24);}
void C2D_DrawTriangle(float x,float y,uint32_t c,float x1,float y1,uint32_t c1,float x2,float y2,uint32_t c2,float z) {
    assert(c==c1&&c==c2);colorSeen=c;depthSeen=z;
    corners.insert(corners.end(),{{x,y},{x1,y1},{x2,y2}});
}
void C2D_DrawRectSolid(float x,float y,float z,float w,float h,uint32_t c) {
    colorSeen=c;depthSeen=z;corners={{x,y},{x+w,y},{x,y+h},{x+w,y+h}};
}
'''
test = r'''
int main() {
    unsigned checks=0;
    for (bool native:{false,true}) for(float size:{18.f,30.f,40.f,48.f})
    for(float offset:{0.f,.25f,.5f}) for(int angle:{0,8192,16384,24576,32768,-8192,-16384})
    for(uint8_t alpha:{uint8_t(0),uint8_t(96),uint8_t(255)}) {
        UiTexture portrait;portrait.native=native;
        mk64_3ds::HudRect box{20+offset,50+offset,size,size};
        float radians=angle*(6.2831853071795864769f/65536.f);
        corners.clear();DrawStandingBacking(portrait,box,radians,alpha);
        DrawStandingLayer(portrait,box,0,0,size,size,.6f,radians,nullptr);
        assert(colorSeen==uint32_t(alpha)<<24 && depthSeen<image.depth);
        // Every backing vertex must land on a corner of the submitted image
        // after undoing that image's transform. This catches the fixed square
        // at diagonal hit angles as well as native-mode center rounding drift.
        for(auto p:corners) {
            float dx=p.x-image.pos.x,dy=p.y-image.pos.y;
            float x=dx*std::cos(image.angle)+dy*std::sin(image.angle);
            float y=-dx*std::sin(image.angle)+dy*std::cos(image.angle);
            assert(std::fabs(std::fabs(x)-image.pos.w*.5f)<.0001f);
            assert(std::fabs(std::fabs(y)-image.pos.h*.5f)<.0001f);
        }
        assert(corners.size()==(angle==0?4:6));++checks;
    }
    std::printf("PASS: %u original/improved portrait backing rotation, alignment and fade cases\n",checks);
}
'''
cpp=args.output/'standing_rotation.cpp'
cpp.write_text(header+'\n'.join(function(n) for n in ('DrawTextureRotated','DrawStandingLayer','DrawStandingBacking'))+test)
binary=args.output/'standing_rotation'
subprocess.run(['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-I',str(port/'include'),str(cpp),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
