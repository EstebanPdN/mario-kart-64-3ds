#!/usr/bin/env python3
"""Exercise the real title upload, variant selection and draw with a stale pack.

Default fixtures are asset-free. Optional owner pack/font paths remain private.
No GPU, game or emulator is run; SDK stubs capture submitted pixels and geometry.
"""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import zlib


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--pack', type=Path)
    parser.add_argument('--font-rgba', type=Path)
    parser.add_argument('--expect-stale', action='store_true')
    args = parser.parse_args()
    root = args.source_root.resolve()
    port = root / 'platform/3ds'
    out = args.output.resolve()
    if out == root or root in out.parents:
        parser.error('Output must be outside Source.')
    out.mkdir(parents=True, exist_ok=True)
    ui = (port / 'source/bottom_ui_3ds.cpp').read_text()
    native = (port / 'source/native_ui_3ds.inc.cpp').read_text()
    start = ui.index('    UiTexture copyright{};')
    end = ui.index('    // Preload the complete, bounded lower HUD', start)
    init = ui[start:end]
    draw = function(ui, '    if (TitleCreditVisible())')
    fixture = out / 'romfs:'
    fixture.mkdir(exist_ok=True)
    pack = fixture / 'improved-textures.bin'
    if pack.exists() or pack.is_symlink():
        pack.unlink()
    if args.pack:
        pack.symlink_to(args.pack.resolve())
        companion = args.pack.resolve().parent / 'native-karts.nsp'
        if companion.is_file():
            link = fixture / companion.name
            if link.exists() or link.is_symlink():
                link.unlink()
            link.symlink_to(companion)
    else:
        name_hash = 14695981039346656037
        for value in b'@title_credit':
            name_hash = ((name_hash ^ value) * 1099511628211) & ((1 << 64) - 1)
        pixels = bytes((240, 240, 240, 255)) * (189 * 18)
        payload = zlib.compress(pixels)
        entry = struct.pack('<QIIIHHHHI', name_hash, 48, len(payload), zlib.crc32(pixels), 189, 18, 189, 18, 0)
        pack.write_bytes(struct.pack('<IIII', 0x3349554D, 1, 1, 32) + entry + payload)
    font = out / 'font.rgba'
    if args.font_rgba:
        font.write_bytes(args.font_rgba.read_bytes())
    else:
        pixels = [0] * (124 * 17)
        for y in range(4, 14):
            for x in range(22, 121):
                shade = 180 + (x + y) % 76
                pixels[y * 124 + x] = 0xff000000 | shade | shade << 8 | shade << 16
        font.write_bytes(struct.pack('<2108I', *pixels))
    assert font.stat().st_size == 124 * 17 * 4
    prelude = r'''
#include "title_credit_native_3ds.hpp"
#include "improved_texture_pack_3ds.hpp"
#include "texture_mode_3ds.hpp"
#include "native_texture_storage_3ds.hpp"
#include "hud_layout_3ds.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
constexpr unsigned kMaxUiTextureDimension=512;
struct C3D_Tex { void* data=nullptr; unsigned width=0,height=0; };
struct Tex3DS_SubTexture { unsigned short width=0,height=0;float left=0,top=0,right=0,bottom=0; };
struct C2D_Image { C3D_Tex* tex; const Tex3DS_SubTexture* subtex; };
struct C2D_ImageTint { float alpha=1; };
struct C2D_DrawParams { struct {float x,y,w,h;} pos;struct{float x,y;}center;float depth,angle; };
constexpr int GPU_LA4=1,GPU_RGBA8=2,GPU_NEAREST=3,GPU_LINEAR=4,GPU_CLAMP_TO_EDGE=5;
unsigned allocations=0,failAllocation=0,liveTextures=0,draws=0;
bool sourceAvailable=true;
Mk64UiTextures3DS mode=MK64_UI_TEXTURES_3DS_IMPROVED;
Mk64UiTextures3DS Mk64Settings3DSGetUiTextures(){return mode;}
bool C3D_TexInit(C3D_Tex* t,unsigned w,unsigned h,int format){
 if(++allocations==failAllocation)return false;
 t->width=w;t->height=h;t->data=std::calloc(w*h,format==GPU_LA4?1:4);
 if(t->data)++liveTextures;return t->data!=nullptr;
}
void C3D_TexDelete(C3D_Tex* t){if(t->data){std::free(t->data);--liveTextures;}*t={};}
void C3D_TexSetFilter(C3D_Tex*,int,int){}
void C3D_TexSetWrap(C3D_Tex*,int,int){}
bool Mk64System3DSCleanDataCache(const void*,size_t){return true;}
void C2D_AlphaImageTint(C2D_ImageTint* tint,float alpha){tint->alpha=alpha;}
'''
    parts = [prelude]
    for signature in ('struct NativeUiVariant', 'struct UiTexture'):
        parts.append(function(ui, signature) + ';')
    parts.append('struct {bool initialized=true;UiTexture titleCredit;std::array<UiTexture,8> retiredTextures;size_t retiredTextureCount=0;} sUi;')
    parts.append('struct {unsigned alpha=0;} sMenuTransition;')
    for signature in ('uint16_t NextPowerOfTwo(', 'constexpr uint32_t MortonOffset8x8(', 'void DeleteTexture(', 'bool RetireTexture(', 'bool ReplaceTexture(', 'void WriteTiledPixel(', 'bool InitBackdropTexture(', 'bool PublishBackdropTexture(', 'C2D_Image TextureImage('):
        parts.append(function(ui, signature))
    for signature in ('mk64_3ds::native_sprite::Image ReadUiPixels(', 'void AddNativeVariant(', 'bool AddPackVariant(', 'NativeUiVariant* NativeVariantFor('):
        parts.append(function(native, signature))
    parts.append(r'''
mk64_3ds::native_sprite::Image source;
bool LoadTexture(const char*,UiTexture& texture){
 if(!sourceAvailable || !InitBackdropTexture(texture,NextPowerOfTwo(source.width),NextPowerOfTwo(source.height)))return false;
 for(unsigned y=0;y<source.height;++y)for(unsigned x=0;x<source.width;++x)WriteTiledPixel(texture.texture,texture.backingWidth,x,y,source.At(x,y));
 texture.width=texture.subTexture.width=source.width;texture.height=texture.subTexture.height=source.height;return true;
}
C2D_DrawParams submitted{};
std::vector<uint32_t> submittedPixels;
float submittedAlpha=0;
void DrawImageWithOpacity(C2D_Image image,const C2D_DrawParams* params,const C2D_ImageTint* tint){
 ++draws;submitted=*params;submittedAlpha=tint?tint->alpha:1;
 submittedPixels.clear();
 auto* p=static_cast<const uint32_t*>(image.tex->data);
 for(unsigned y=0;y<image.subtex->height;++y)for(unsigned x=0;x<image.subtex->width;++x){
  size_t at=(size_t(y/8)*(image.tex->width/8)+x/8)*64+MortonOffset8x8(x%8,y%8);
  submittedPixels.push_back(__builtin_bswap32(p[at]));
 }
}
''')
    parts.append(function(ui, 'void DrawTexture('))
    parts.append('void InitializeTitle(){' + init + '}')
    parts.append('void DrawTitle(){' + draw[draw.index('{') + 1:-1] + '}')
    parts.append(function(ui, 'extern "C" bool Mk64BottomUI3DSHasTitleCopyright('))
    parts.append(r'''
void Clear(){DeleteTexture(sUi.titleCredit);for(size_t i=0;i<sUi.retiredTextureCount;++i)DeleteTexture(sUi.retiredTextures[i]);sUi.retiredTextureCount=0;assert(liveTextures==0);}
int main(int argc,char**argv){
 source={124,17,std::vector<uint32_t>(124*17)};
 std::ifstream(argv[1],std::ios::binary).read(reinterpret_cast<char*>(source.pixels.data()),source.pixels.size()*4);
 mk64_3ds::improved::LocalKartAliasesEnabled()=false;
 auto old=mk64_3ds::improved::LocalPack().Load("@title_credit",145,32);
 assert(old && old.width==189 && old.height==18);
 const auto expected=mk64_3ds::MakeNativeTitleCredits(source);
 assert(expected.Valid() && expected.width>=140 && expected.width<=166 && expected.height==32);
 for(unsigned first:{0U,16U}){bool ink=false;for(unsigned y=first;y<first+16;++y)for(unsigned x=0;x<expected.width;++x)ink|=expected.At(x,y)>>24;assert(ink);}
 InitializeTitle();assert(Mk64BottomUI3DSHasTitleCopyright());
 for(auto setting:{MK64_UI_TEXTURES_3DS_ORIGINAL,MK64_UI_TEXTURES_3DS_MENU,MK64_UI_TEXTURES_3DS_IMPROVED}){
  mode=setting;
  for(unsigned alpha:{0U,128U,255U}){
   sMenuTransition.alpha=alpha;draws=0;DrawTitle();assert(draws==1);
   if(submitted.pos.w!=expected.width || submitted.pos.h!=expected.height || submittedPixels!=expected.pixels){
    std::cerr<<"FAIL stale title variant: mode="<<mode<<" submitted="<<submitted.pos.w<<"x"<<submitted.pos.h<<"; both lines required\n";Clear();return 17;
   }
   assert(submitted.pos.x==std::floor((400-expected.width)*.5f) && submitted.pos.y==202);
   assert(std::abs(submittedAlpha-(255-alpha)/255.0f)<.00001f);
  }
  std::cout<<"PASS mode="<<mode<<" "<<expected.width<<"x"<<expected.height<<"; copyright first, port second; exact pixels and fade\n";
 }
 {std::ofstream capture(argv[2],std::ios::binary);const uint32_t w=uint32_t(submitted.pos.w),h=uint32_t(submitted.pos.h);capture.write(reinterpret_cast<const char*>(&w),4);capture.write(reinterpret_cast<const char*>(&h),4);capture.write(reinterpret_cast<const char*>(submittedPixels.data()),submittedPixels.size()*4);}
 Clear();sourceAvailable=false;InitializeTitle();assert(!Mk64BottomUI3DSHasTitleCopyright());Clear();
 sourceAvailable=true;allocations=0;failAllocation=2;InitializeTitle();assert(!Mk64BottomUI3DSHasTitleCopyright());Clear();
 std::cout<<"PASS missing source and replacement allocation failure preserve stock-copyright eligibility\n";
}
''')
    cpp = out / 'title-pipeline.cpp'
    cpp.write_text('\n'.join(parts))
    binary = out / 'title-pipeline'
    command = ['clang++', '-std=c++20', '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I', str(port / 'include'), str(cpp), '-lz', '-o', str(binary)]
    commands = [command, [str(binary), str(font), str(out / 'submitted.rgba')]]
    (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    subprocess.run(command, check=True)
    result = subprocess.run(commands[1], cwd=out, capture_output=True, text=True)
    (out / 'results.log').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    expected_code = 17 if args.expect_stale else 0
    if result.returncode != expected_code:
        raise SystemExit(f'Expected exit {expected_code}, got {result.returncode}')
    (out / 'summary.json').write_text(json.dumps({'passed': True, 'expected_exit': expected_code, 'actual_exit': result.returncode, 'source_root': str(root), 'scope': 'Actual composition/upload/pack lookup/variant selection/draw; SDK capture only.'}, indent=2) + '\n')


if __name__ == '__main__':
    main()
