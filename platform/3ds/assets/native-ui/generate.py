#!/usr/bin/env python3
"""Author original native-size HUD lettering and the port credit, pixel by pixel.

No ROM, system font, downloaded artwork, or image library is used to build the
header. PNG contact sheets are optional and require Pillow. Pixel coordinates,
letter contours, colors, outlines and shadows are explicit and reproducible.
"""
import argparse
from pathlib import Path

# HUD outlines are polygon strokes, rasterized directly at each final size.
# Coordinates are in a 6x10 design space; the outline is not a resized bitmap.
STROKES = {
 'L':[((0,0),(2,0),(2,8),(6,8),(6,10),(0,10))],
 'A':[((0,10),(2,0),(4,0),(6,10),(4,10),(3,3),(2,10)),((1,6),(5,6),(5,8),(1,8))],
 'P':[((0,0),(5,0),(6,2),(6,5),(5,6),(2,6),(2,10),(0,10)),((2,2),(4,2),(4,4),(2,4))],
 'T':[((0,0),(6,0),(6,2),(4,2),(4,10),(2,10),(2,2),(0,2))],
 'I':[((0,0),(6,0),(6,2),(4,2),(4,8),(6,8),(6,10),(0,10),(0,8),(2,8),(2,2),(0,2))],
 'M':[((0,10),(0,0),(2,0),(3,4),(4,0),(6,0),(6,10),(4,10),(4,4),(3,7),(2,4),(2,10))],
 'E':[((0,0),(6,0),(6,2),(2,2),(2,4),(5,4),(5,6),(2,6),(2,8),(6,8),(6,10),(0,10))],
 'O':[((1,0),(5,0),(6,1),(6,9),(5,10),(1,10),(0,9),(0,1)),((2,2),(4,2),(4,8),(2,8))],
 '/':[((0,10),(4,0),(6,0),(2,10))],
}
# Holes are explicitly cut out after their surrounding stroke.
HOLES = {'P':{1},'O':{1}}
DIGITS = [
 ['01110','11011','11011','11011','11011','11011','01110'],
 ['00110','01110','00110','00110','00110','00110','01111'],
 ['01110','11011','00011','00110','01100','11000','11111'],
 ['11110','00011','00011','01110','00011','00011','11110'],
 ['11011','11011','11011','11111','00011','00011','00011'],
 ['11111','11000','11000','11110','00011','00011','11110'],
 ['01110','11000','11000','11110','11011','11011','01110'],
 ['11111','00011','00011','00110','00110','01100','01100'],
 ['01110','11011','11011','01110','11011','11011','01110'],
 ['01110','11011','11011','01111','00011','00011','01110'],
]

def inside(x,y,points):
    yes=False
    for (ax,ay),(bx,by) in zip(points,points[1:]+points[:1]):
        if (ay>y)!=(by>y) and x < (bx-ax)*(y-ay)/(by-ay)+ax: yes=not yes
    return yes

def ink_text(word,w,h):
    mask=set(); margin=1; count=len(word); advance=(w-2)/count
    for i,c in enumerate(word):
        for y in range(1,h-1):
            for x in range(w):
                # A restrained rightward slant matches the racing lettering.
                px=(x+.5-margin-i*advance-(h-2-y)*.10)*6/(advance-1)
                py=(y-.5)*10/(h-2)
                strokes=STROKES[c]; hit=any(inside(px,py,list(p)) for j,p in enumerate(strokes) if j not in HOLES.get(c,set()))
                if any(inside(px,py,list(strokes[j])) for j in HOLES.get(c,set())): hit=False
                if hit:mask.add((x,y))
    return mask

def digit(n,w,h):
    mask=set()
    for y in range(h):
        row=DIGITS[n][min(6,y*7//h)]
        for x in range(w):
            # Direct native-grid stencil: no anti-aliasing or filtered image.
            xx=x-(h-1-y)//6
            if xx>=0 and xx<w and row[min(4,xx*5//w)]=='1':mask.add((x,y))
    return mask

def paint(w,h,mask,top,bottom,outline=False):
    out=[0]*(w*h)
    def put(x,y,c):
        if 0<=x<w and 0<=y<h:out[y*w+x]=c
    if outline:
        for x,y in mask:
            for dx,dy in [(-1,0),(1,0),(0,-1),(0,1),(1,1)]:put(x+dx,y+dy,0xff151515)
            put(x+1,y+1,0xff080808)
    for x,y in mask:
        t=max(0,min(1,(y-1)/max(1,h-3)))
        rgb=[round(a+(b-a)*t) for a,b in zip(top,bottom)]
        put(x,y,0xff000000|rgb[0]|rgb[1]<<8|rgb[2]<<16)
    return out

def artwork():
    art={}
    def add(name,w,h,mask,colors,outline=False):art[name]=(w,h,paint(w,h,mask,*colors,outline))
    lap=((250,255,255),(183,135,239));time=((254,255,20),(20,162,12));numbers=((255,249,0),(239,86,0))
    add('Lap',40,10,ink_text('LAP',30,10),lap)
    add('LapTop',48,12,ink_text('LAP',36,12),lap)
    # Independently drawn contours follow the original TIME silhouette:
    # a broad T, solid narrow I, open M crotch and three E bars. The large
    # transparent bearings of the archive labels are retained on the canvas.
    def original_word(word):
        widths={'T':8,'I':3,'M':9,'E':7,'O':6,'A':6,'L':5}
        masks=set();x0=0
        for c in word:
            gw=5 if word=='TOTAL' else widths[c]
            for y in range(1,15):
                for x in range(gw):
                    px=(x+.5-(14-y)*.10)*6/gw;py=(y-.5)*10/14
                    strokes=STROKES[c]
                    if c=='I':hit=0<=px<5
                    else:
                        hit=any(inside(px,py,list(poly)) for j,poly in enumerate(strokes) if j not in HOLES.get(c,set()))
                        if any(inside(px,py,list(strokes[j])) for j in HOLES.get(c,set())):hit=False
                    if hit:masks.add((x0+x,y))
            x0+=gw+1
        return masks
    add('Time',32,16,original_word('TIME'),time)
    add('Total',32,16,original_word('TOTAL'),time)
    for top in (False,True):
        w,h=(41,20) if top else (34,17)
        for lap_index in range(1,4):
            face=set();dw=9 if top else 8;dh=h-3
            for n,ox in [(lap_index,1),(3,w-dw-2)]:face|={(x+ox,y+1) for x,y in digit(n,dw,dh)}
            for y in range(1,h-1):
                x=round(w*.35+(h-2-y)*.43);face.add((x,y));face.add((x+1,y))
            add(f'LapCount{lap_index}'+('Top' if top else ''),w,h,face,lap)
    # Twelve individually authored 8x16 cells, including separate quote marks.
    mask=set()
    for n in range(10):mask|={(n*8+x,y+1) for x,y in digit(n,7,13)}
    for y in range(2,6):
        for x in (1,2):mask.add((80+x,y))
        for x in (1,2,4,5):mask.add((88+x,y))
    add('TimerDigits',96,16,mask,numbers)
    return art

def write_header(art,path,check=False):
    lines=['// Generated by assets/native-ui/generate.py. Original port artwork.', '#pragma once','#include <cstdint>','namespace mk64_3ds::native_art {','struct Sprite { unsigned width, height; const uint32_t* pixels; };']
    for name,(w,h,pixels) in art.items():
        lines.append(f'inline constexpr uint32_t k{name}Pixels[] = {{')
        for i in range(0,len(pixels),12):lines.append('    '+','.join(f'0x{p:08x}U' for p in pixels[i:i+12])+',')
        lines+=['};',f'inline constexpr Sprite k{name}{{{w},{h},k{name}Pixels}};']
    lines.append('} // namespace mk64_3ds::native_art')
    content='\n'.join(lines)+'\n'
    if check:
        if not path.exists() or path.read_text()!=content:raise SystemExit('Generated HUD art is stale; regenerate it.')
    elif not path.exists() or path.read_text()!=content:path.write_text(content)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--preview',type=Path)
    p.add_argument('--check',action='store_true',help='Verify the generated header without writing it.')
    a=p.parse_args();art=artwork()
    write_header(art,Path(__file__).resolve().parents[2]/'source/art/native_hud_art_3ds.hpp',a.check)
    if a.preview:
        from PIL import Image,ImageDraw
        a.preview.mkdir(parents=True,exist_ok=True);sheet=Image.new('RGBA',(620,1200),(42,42,42,255));d=ImageDraw.Draw(sheet);y=6
        for name,(w,h,pixels) in art.items():
            image=Image.new('RGBA',(w,h));image.putdata([(v&255,v>>8&255,v>>16&255,v>>24) for v in pixels]);image.save(a.preview/f'{name}.png')
            d.text((5,y),f'{name} {w}x{h}',fill='white');sheet.alpha_composite(image.resize((w*3,h*3),Image.Resampling.NEAREST),(5,y+13));y+=h*3+18
        sheet.crop((0,0,620,y)).save(a.preview/'native-lettering.png')
    print(f'{"Verified" if a.check else "Generated"} {len(art)} original native-size sprites.')
if __name__=='__main__':main()
