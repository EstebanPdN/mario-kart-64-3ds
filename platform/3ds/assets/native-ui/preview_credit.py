#!/usr/bin/env python3
"""Render the actual runtime credit recipes from the owner's copyright font.

Generated images contain private archive-derived pixels; output must be outside
Source. No extracted font is embedded in the public source or generated header.
"""
import argparse,ctypes,struct,subprocess,sys,zipfile
from pathlib import Path
from PIL import Image,ImageDraw
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--archive',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
source=Path(__file__).resolve().parents[4];out=a.output.resolve()
if out==source or source in out.parents:p.error('Archive-derived previews must stay outside Source.')
out.mkdir(parents=True,exist_ok=True)
b=zipfile.ZipFile(a.archive).read('textures/player_selection/copyright_1996');kind,w,h,size=struct.unpack_from('<IIII',b,64)
assert(kind,w,h)==(2,124,17) and len(b)>=80+size
pixels=[]
for i in range(w*h):
 v=int.from_bytes(b[80+i*2:82+i*2],'big');r=((v>>11)&31)*255//31;g=((v>>6)&31)*255//31;blue=((v>>1)&31)*255//31;alpha=(v&1)*255
 pixels.append(r|g<<8|blue<<16|alpha<<24)
cpp=out/'credit-preview.cpp';cpp.write_text('''#include "title_credit_native_3ds.hpp"
extern "C" unsigned credit(const uint32_t* p,uint32_t* output,unsigned* height) {
 mk64_3ds::native_sprite::Image source{124,17,std::vector<uint32_t>(p,p+124*17)};
 auto image=mk64_3ds::MakeNativeTitleCredit(source);std::copy(image.pixels.begin(),image.pixels.end(),output);*height=image.height;return image.width;
}
''')
lib=out/('credit-preview.dylib' if sys.platform=='darwin' else 'credit-preview.so')
subprocess.run(['clang++','-std=c++17','-O2','-shared','-fPIC','-I',str(Path(__file__).resolve().parents[2]/'include'),str(cpp),'-o',str(lib)],check=True)
u32=ctypes.c_uint32;fn=ctypes.CDLL(str(lib)).credit;fn.argtypes=[ctypes.POINTER(u32),ctypes.POINTER(u32),ctypes.POINTER(ctypes.c_uint)];fn.restype=ctypes.c_uint
result=(u32*(512*32))();height=ctypes.c_uint();width=fn((u32*len(pixels))(*pixels),result,ctypes.byref(height));height=height.value
im=Image.new('RGBA',(width,height));im.putdata([(v&255,v>>8&255,v>>16&255,v>>24) for v in result[:width*height]]);im.save(out/'credit-native.png')
reference=Image.new('RGBA',(124,17));reference.putdata([(v&255,v>>8&255,v>>16&255,v>>24) for v in pixels]);reference=reference.crop((61,0,123,17))
# Both rows use the same six-times nearest zoom, without independent resizing.
sheet=Image.new('RGBA',(max(width,reference.width)*6+20,145+height*6+10),(38,38,38,255));draw=ImageDraw.Draw(sheet)
draw.text((10,4),'Original Nintendo lettering (same 6x zoom)',fill='white');sheet.alpha_composite(reference.resize((reference.width*6,102),Image.Resampling.NEAREST),(10,20))
draw.text((10,125),'Credit composed from the original lettering and stroke recipes',fill='white');sheet.alpha_composite(im.resize((width*6,height*6),Image.Resampling.NEAREST),(10,145));sheet.save(out/'credit-native-comparison.png')
print(f'Composed {width}x{height} credit from the runtime font recipes.')
