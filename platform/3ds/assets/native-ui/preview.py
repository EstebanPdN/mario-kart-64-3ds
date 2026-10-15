#!/usr/bin/env python3
"""Bilinearly scale private archive art using the console pixel code.

Requires Pillow and a host C++17 compiler. Output must be outside the source
checkout: course/character pixels belong to the owner's archive and are never
redistributable port artwork. The console performs the same scaling at
resource load, so no PC-generated texture pack needs to be shipped or installed.
"""
import argparse,ctypes,hashlib,json,struct,subprocess,sys,zipfile
from pathlib import Path
from PIL import Image,ImageDraw

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--archive',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();source=Path(__file__).resolve().parents[4]
if out==source or source in out.parents:p.error('Private archive previews must be outside the source checkout.')
out.mkdir(parents=True,exist_ok=True)
include=Path(__file__).resolve().parents[2]/'include'
cpp=out/'native-preview.cpp';cpp.write_text('''#include "native_sprite_3ds.hpp"
extern "C" bool scale_image(const uint32_t* src,unsigned sw,unsigned sh,uint32_t* dst,unsigned w,unsigned h) {
 mk64_3ds::native_sprite::Image source{sw,sh,std::vector<uint32_t>(src,src+sw*sh)};
 auto image=mk64_3ds::native_sprite::ScaleBilinear(source,w,h);
 if(!image.Valid())return false;std::copy(image.pixels.begin(),image.pixels.end(),dst);return true;
}
''')
lib=out/('native-preview.dylib' if sys.platform=='darwin' else 'native-preview.so')
subprocess.run(['clang++','-std=c++17','-O2','-shared','-fPIC','-I',str(include),str(cpp),'-o',str(lib)],check=True)
fn=ctypes.CDLL(str(lib)).scale_image;u32=ctypes.c_uint32
fn.argtypes=[ctypes.POINTER(u32),ctypes.c_uint,ctypes.c_uint,ctypes.POINTER(u32),ctypes.c_uint,ctypes.c_uint];fn.restype=ctypes.c_bool
z=zipfile.ZipFile(a.archive)
def rgba16(v):return (((v>>11)&31)*255//31,((v>>6)&31)*255//31,((v>>1)&31)*255//31,(v&1)*255)
def decode(name,palette=None):
    data=z.read(name);kind,w,h,size=struct.unpack_from('<IIII',data,64);raw=data[80:]
    if len(raw)<size:raise ValueError(f'Truncated texture: {name}')
    colors=None
    if palette:
        pr=z.read(palette)[80:];colors=[rgba16(int.from_bytes(pr[i:i+2],'big')) for i in range(0,len(pr),2)]
    pixels=[]
    for i in range(w*h):
        if kind==1:c=tuple(raw[i*4:i*4+4])
        elif kind==2:c=rgba16(int.from_bytes(raw[i*2:i*2+2],'big'))
        elif kind==4:c=colors[raw[i]]
        elif kind==5:
            v=((raw[i//2]>>(4 if i%2==0 else 0))&15)*17;c=(v,v,v,v)
        elif kind==8:c=((raw[i]>>4)*17,)*3+((raw[i]&15)*17,)
        elif kind==9:c=(raw[i*2],)*3+(raw[i*2+1],)
        else:raise ValueError(f'Unsupported texture type: {kind}')
        pixels.append(c)
    image=Image.new('RGBA',(w,h));image.putdata(pixels);return image

def scale_image(image,w,h):
    values=[r|g<<8|b<<16|alpha<<24 for r,g,b,alpha in image.get_flattened_data()]
    src=(u32*len(values))(*values);dst=(u32*(w*h))()
    if not fn(src,image.width,image.height,dst,w,h):raise ValueError('Native scaling failed')
    result=Image.new('RGBA',(w,h));result.putdata([(v&255,v>>8&255,v>>16&255,v>>24) for v in dst]);return result

entries=[];portraits=[];maps=[]
characters=['mario','luigi','yoshi','toad','donkey_kong','wario','peach','bowser']
for c in characters:
    name=f'textures/common_data/common_texture_portrait_{c}'
    image=decode(name,f'textures/common_data/common_tlut_portrait_{c}')
    for size in (18,30,40,48):
        result=scale_image(image,size,size);file=f'portrait-{c}-{size}.png';result.save(out/file)
        entries.append({'source':name,'output':file,'size':result.size,'method':'premultiplied-alpha bilinear scaling'})
        if size==40:portraits.append((c,image,result))
for name in z.namelist():
    if not name.startswith('textures/other_textures/minimap_'):continue
    image=decode(name);scale=min(149/image.width,184/image.height);w,h=round(image.width*scale),round(image.height*scale)
    result=scale_image(image,w,h);file=name.split('/')[-1]+'.png';result.save(out/file)
    entries.append({'source':name,'output':file,'size':result.size,'method':'premultiplied-alpha bilinear scaling'})
    maps.append((name.split('/')[-1][8:],image,result))

def sheet(rows,filename,cellw,cellh):
    cols=4;result=Image.new('RGBA',(cols*cellw,((len(rows)+cols-1)//cols)*cellh),(32,32,32,255));d=ImageDraw.Draw(result)
    for i,(name,before,after) in enumerate(rows):
        x=(i%cols)*cellw;y=(i//cols)*cellh;d.text((x+5,y+4),name,fill='white')
        # Review includes a 1:1 sprite and a nearest 3x inspection copy.
        result.alpha_composite(after,(x+5,y+20));big=after.resize((after.width*2,after.height*2),Image.Resampling.NEAREST)
        result.alpha_composite(big,(x+5,y+25+after.height))
    result.save(out/filename)
sheet(portraits,'portraits-native.png',180,160)
sheet(maps,'maps-native.png',310,590)
(out/'inventory.json').write_text(json.dumps({'archive_sha256':hashlib.sha256(a.archive.read_bytes()).hexdigest(),'entries':entries},indent=2)+'\n')
print(f'Rebuilt {len(entries)} private native-size images; no archive pixels were written to Source.')
