#!/usr/bin/env python3
"""Compare generated kart preparation with the preserved baseline, without a game run."""
import argparse,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--before',type=Path,required=True)
p.add_argument('--after',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1]
before=a.before.read_text();after=a.after.read_text()
def fn(s,name):
 start=s.index('void '+name+'(');brace=s.index('{',start);depth=1;i=brace+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
def mapped(s):
 return s.replace('[screenId - 1][playerId - 4].unk_00','[screenId - 2][playerId + 4].unk_00').replace('load_kart_texture(player, playerId - 4, screenId, screenId - 1,','load_kart_texture(player, playerId + 4, screenId, screenId - 2,')
def tokens(s):
 s=re.sub(r'/\*.*?\*/|//[^\n]*','',s,flags=re.S)
 return re.findall(r'[A-Za-z_]\w*|\d+|[^\s]',s)
blocks={}
for name,begin,end,opening in (
 ('render_kart','    load_kart_palette(', '    mtxf_translate_rotate(mtx, sp154, sp14C);','    FrameInterpolation_RecordOpenChild("player_kart"'),
 ('render_ghost','#ifdef AVOID_UB\n    gPlayerPalette =','    mtxf_translate_rotate(mtx, spDC, spD4);','    FrameInterpolation_RecordOpenChild("player_ghost"')):
 b=mapped(fn(before,name));n=fn(after,name)
 block=b[b.index(begin):b.index(end)]
 assert block in n, name+' must preserve exact palette and texture ownership work'
 guard='    if (Mk64Graphics3DSSkipVisualPreparation()) return;\n'
 assert n.count(guard)==1
 # Undo only the deliberate move and guard: every drawn expression must match.
 restored=n.replace(block,'',1).replace(guard,'',1).replace(end,block+end,1)
 assert tokens(restored)==tokens(b),name+' changed drawn geometry'
 assert n.index(block)<n.index(guard)<n.index(opening)
 blocks[name]=block
for name in ('render_player_shadow','render_player_shadow_credits','func_80025DE8','render_player_ice_reflection'):
 b=fn(before,name);n=fn(after,name)
 n=re.sub(r'\n    if \(Mk64Graphics3DSSkipVisualPreparation\(\)\) (?:return;|\{ \+\+gMatrixEffectCount; return; \})','',n,count=1)
 assert tokens(n)==tokens(b),name+' changed drawn geometry'
for name in ('init_render_player','render_players','render_player','update_wheel_palette','func_80023BF0'):
 assert tokens(fn(before,name))==tokens(fn(after,name)),name+' must retain simulation and animation work'
header=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "presentation_skip_3ds.hpp"
#define AVOID_UB 1
using s8=int8_t;
struct Player {};
struct Palette { unsigned base=0,wheel=0; };
struct Texture { const char* unk_00=nullptr; };
int D_801651D0[4][8]{};
Palette gPlayerPalettesList[2][4][8]{};
Texture gEncodedKartTexture[2][2][8]{};
Palette* gPlayerPalette=nullptr;
const char* sKartTexture=nullptr;
unsigned loads=0;
char samples[2][2][8]{};
void load_kart_palette(Player*,int player,int screen,int buffer) {gPlayerPalettesList[buffer][screen][player].base=123;}
void load_kart_texture(Player*,int player,int,int screen,int buffer) {
 assert(player>=0 && player<8 && screen>=0 && screen<2 && buffer>=0 && buffer<2);
 gEncodedKartTexture[buffer][screen][player].unk_00=&samples[buffer][screen][player];++loads;
}
'''
code=header
for name,block in blocks.items():
 code+=f'void {name}(Player* player, s8 playerId, s8 screenId) {{\n{block}\n}}\n'
code+=r'''
int main() {
 for(bool suppress:{false,true}) for(bool limiter:{false,true})
 for(bool interpolation:{false,true}) for(bool midpoint:{false,true}) {
  bool skip=mk64_3ds::SkipVisualPreparation(suppress,limiter,interpolation,midpoint);
  assert(skip == (suppress && (limiter || !interpolation || !midpoint)));
 }
 Player player;
 for(int buffer=0;buffer<2;++buffer) for(int screen=0;screen<4;++screen)
 for(int id=0;id<(screen>=2?4:8);++id) for(int empty=0;empty<2;++empty) {
  for(auto& b:gEncodedKartTexture) for(auto& s:b) for(auto& p:s) p.unk_00=nullptr;
  int bank=screen>=2?screen-2:screen,slot=screen>=2?id+4:id;
  D_801651D0[screen][id]=buffer;
  const char* expected=&samples[buffer][bank][slot];
  if(!empty)gEncodedKartTexture[buffer][bank][slot].unk_00=expected;
  gPlayerPalettesList[buffer][screen][id].wheel=456;
  unsigned previous=loads;
  render_kart(&player,id,screen);
  assert(sKartTexture==expected && loads==previous+empty);
  assert(gPlayerPalette==&gPlayerPalettesList[buffer][screen][id]);
  assert(gPlayerPalette->base==123 && gPlayerPalette->wheel==456);
  sKartTexture=nullptr;gPlayerPalette=nullptr;
  render_ghost(&player,id,screen);
  assert(sKartTexture==expected && gPlayerPalette==&gPlayerPalettesList[buffer][screen][id]);
 }
 std::puts("PASS: six drawn kart functions unchanged; simulation/animation retained; all 1P-4P palette/buffer mappings and empty-slot repairs pass");
}
'''
code=code.replace('#include <cassert>','#include <cassert>\n#include <initializer_list>')
cpp=a.output/'kart-preparation.cpp';cpp.write_text(code);exe=a.output/'kart-preparation'
subprocess.run(['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-I',str(port/'include'),str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
