#!/usr/bin/env python3
"""Rasterize hand-authored course curves directly onto the final native grid.

Paths are editable drawing instructions, not sampled bitmap contours. No ROM,
reference image or installed font is an input. The source-coordinate frame is
retained, including its bearings, for finish/player marker registration.
"""
import argparse
import re
from pathlib import Path
from PIL import Image, ImageDraw

# Widths are in the original coordinate frame, not in output pixels. Connected
# cubic segments share endpoints. Separate strokes preserve bridges and jumps;
# filled regions preserve deliberately wider roads and battle platforms.
MAPS = {
    'mario_raceway': dict(frame=(64,64), strokes=[(2.6,
        'M 6 35 L 6 14 C 6 10 9 9 13 8 L 20 6 C 24 5 27 7 25 11 '
        'C 22 16 22 19 27 22 C 33 25 39 28 43 26 C 46 24 47 19 51 16 '
        'C 55 11 61 15 60 19 C 59 23 54 25 55 30 C 55 33 58 35 58 39 '
        'L 58 55 C 58 59 54 59 50 58 C 45 57 42 56 45 52 '
        'C 49 47 48 44 44 42 C 41 40 38 40 33 40 L 11 40 C 7 40 6 39 6 35 Z')]),
    'choco_mountain': dict(frame=(64,64), strokes=[(2.8,
        'M 19 19 L 19 13 C 19 10 20 9 24 8 C 28 7 29 12 33 12 '
        'L 39 12 C 44 12 47 15 47 18 C 47 22 43 23 38 24 L 15 24 '
        'C 9 24 4 28 2 32 C 1 37 2 40 6 43 C 6 48 8 52 12 56 '
        'C 15 60 18 59 24 59 L 34 59 C 40 59 48 56 54 52 '
        'C 59 48 62 44 61 40 C 61 36 57 34 54 36 C 50 38 51 42 51 47 '
        'C 51 53 45 57 38 57 C 33 57 32 54 29 51 L 21 43 '
        'C 19 41 19 39 19 36 L 19 19 Z')]),
    'bowsers_castle': dict(frame=(64,64), strokes=[(1.9,
        'M 11 55 L 11 23 C 11 20 15 17 18 17 L 31 17 L 34 13 '
        'L 34 8 L 33 5 L 34 2 L 51 2 C 53 2 54 3 54 6 L 54 16 '
        'C 54 19 51 19 48 19 L 43 19 C 41 19 41 21 41 24 L 41 35 '
        'L 29 35 C 27 35 27 36 27 38 L 27 40 L 43 40 '
        'C 45 40 46 41 46 44 L 46 57 C 46 59 48 62 50 62 '
        'C 54 62 52 57 49 57 L 12 57 C 11 57 11 56 11 55 Z')], fills=[
        'M 32 0 L 51 0 L 52 4 L 32 4 Z',
        'M 31 8 C 30 11 31 14 33 15 L 36 13 L 36 9 Z',
        'M 23 15 L 34 15 C 38 15 38 20 34 20 L 23 20 Z',
        'M 42 17 L 54 17 L 54 20 L 43 21 L 41 20 Z',
        'M 41 24 C 45 23 45 28 41 29 C 37 29 37 25 41 24 Z',
        'M 13 55 L 19 54 L 22 55 L 19 57 L 22 59 L 12 59 Z',
        'M 24 54 L 38 54 L 38 59 L 31 59 L 29 58 L 27 59 L 24 59 Z']),
    'banshee_boardwalk': dict(frame=(64,64), strokes=[(2.0,
        'M 6 33 L 6 38 C 6 39 7 40 9 40 L 39 40 C 42 40 43 41 43 44 '
        'L 43 58 C 43 60 46 62 49 62 L 52 62 C 54 62 55 61 55 58 '
        'L 55 30 L 44 30 L 44 22 L 56 10 C 59 7 55 7 53 7 '
        'L 31 7 C 28 7 27 7 25 9 L 18 16 L 17 21 L 6 33 Z')], fills=[
        'M 16 38 L 22 38 L 22 35 L 32 35 L 32 38 L 39 38 L 39 41 L 16 41 Z']),
    'yoshi_valley': dict(frame=(64,64), strokes=[(1.9,
        'M 36 58 C 31 58 27 55 22 52 C 16 50 12 44 10 39 '
        'C 8 34 9 30 11 27 C 14 23 12 19 8 16 C 4 14 3 10 5 8 '
        'C 8 4 10 7 12 11 L 16 21 C 18 26 20 29 25 32 '
        'L 32 39 C 35 43 39 47 42 45 C 46 43 43 37 42 34 '
        'C 39 28 38 25 40 21 L 40 13 L 50 9 C 55 7 59 12 60 20 '
        'C 62 28 62 37 61 44 C 60 49 59 51 55 52 L 48 56 '
        'C 44 59 40 60 36 58 Z'), (1.8,
        'M 22 22 L 20 13 C 19 9 18 5 21 4 C 25 3 26 6 29 7 '
        'C 34 8 38 9 40 13 L 40 23 L 33 23 C 29 23 29 18 26 18 '
        'C 23 18 24 24 26 27 L 31 32 L 30 38 C 25 41 21 39 18 36 '
        'L 17 29 C 17 26 20 26 21 28 L 22 36'), (1.7,
        'M 24 6 L 24 18 L 16 25 L 12 31 C 11 36 12 40 17 43 '
        'M 30 8 L 29 16 L 26 18 M 31 32 L 40 32')], fills=[
        'M 17 47 C 18 45 22 46 23 48 C 25 52 21 53 18 51 Z']),
    'frappe_snowland': dict(frame=(64,64), strokes=[(2.4,
        'M 10 29 L 10 48 C 10 53 13 55 17 55 C 21 56 22 52 25 52 '
        'C 28 52 29 57 32 57 L 45 57 C 51 57 56 60 59 57 '
        'C 63 53 55 49 51 50 L 42 52 C 37 53 35 50 35 47 '
        'L 35 32 C 35 28 39 27 43 26 C 47 25 50 25 50 21 '
        'C 50 17 46 15 46 11 L 46 6 C 46 1 40 1 39 5 '
        'L 38 10 C 35 13 29 15 25 13 C 22 11 20 14 18 18 '
        'C 15 22 12 25 10 27 L 10 29 Z')], fills=[
        'M 9 31 L 13 34 L 13 50 L 12 54 L 9 52 L 8 48 L 8 40 Z']),
    'koopa_troopa_beach': dict(frame=(64,64), strokes=[(2.4,
        'M 17 17 C 17 9 23 5 28 5 L 32 2 C 35 0 38 2 38 6 '
        'C 38 9 42 10 41 13 L 39 15 L 39 32 C 39 37 44 39 48 42 '
        'C 52 46 48 51 45 54 C 43 58 40 62 34 62 '
        'C 29 62 24 60 23 57 C 21 54 25 52 29 50 '
        'C 34 47 35 45 33 42 L 31 38 L 23 38 C 19 40 17 38 18 34 '
        'L 19 30 C 20 27 16 25 16 22 L 17 17 Z')], fills=[
        'M 24 3 L 33 0 L 38 0 L 40 8 L 36 10 L 30 9 Z',
        'M 19 27 L 24 30 L 23 33 L 30 36 L 31 39 L 21 39 L 20 35 Z',
        'M 42 50 L 48 49 L 47 54 L 44 55 Z',
        'M 25 56 C 29 61 36 60 40 57 L 44 56 C 42 62 37 64 32 64 L 27 61 Z'], cuts=[
        'M 31 5 C 31 3 34 2 35 4 L 35 7 L 32 8 Z',
        'M 18 35 L 20 33 L 22 36 L 21 39 L 18 38 Z']),
    'royal_raceway': dict(frame=(64,64), strokes=[(2.0,
        'M 15 38 L 5 43 C 2 45 3 48 7 49 C 11 50 12 52 13 55 '
        'C 14 59 16 58 17 55 C 18 53 20 54 23 55 L 30 56 '
        'C 35 56 36 54 36 51 L 36 27 C 36 25 36 24 34 22 '
        'L 25 16 C 22 14 20 14 19 16 C 17 18 21 21 22 24 '
        'C 24 28 21 30 18 28 L 11 19 C 10 17 11 16 13 14 '
        'L 18 10 C 22 7 24 8 28 11 L 32 14 C 34 16 36 15 40 13 '
        'L 53 8 C 59 5 62 10 57 13 L 25 33')]),
    'luigi_raceway': dict(frame=(64,96), strokes=[(2.0,
        'M 43 20 C 43 13 39 9 33 9 C 27 9 20 14 20 20 '
        'C 19 26 23 29 28 32 C 35 36 42 40 42 47 L 42 55 '
        'C 42 62 35 64 30 69 C 25 74 24 76 26 80 '
        'C 28 87 36 87 40 82 C 44 78 43 74 43 69 L 43 20 Z')]),
    'moo_moo_farm': dict(frame=(64,64), strokes=[(3.8,
        'M 18 18 C 17 12 18 8 23 5 C 28 1 33 2 36 6 '
        'C 38 10 45 9 46 14 C 48 18 44 21 43 24 '
        'C 40 28 44 31 46 34 C 48 37 47 41 47 45 '
        'L 47 49 C 46 55 42 59 36 60 C 29 63 24 58 21 53 '
        'C 18 48 18 43 18 37 L 18 18 Z')], fills=[
        'M 17 42 C 18 52 26 61 33 61 C 40 61 46 57 47 50 '
        'L 49 48 C 51 58 44 64 35 64 C 25 64 17 54 16 45 Z']),
    'toads_turnpike': dict(frame=(128,64), strokes=[(1.9,
        'M 58 18 C 58 11 52 4 45 4 L 35 4 C 28 4 24 10 24 17 '
        'C 24 24 29 30 36 30 L 90 30 C 98 30 103 36 103 43 '
        'C 103 51 98 59 89 59 L 72 59 C 65 59 58 54 58 47 L 58 18 Z')]),
    # This is the approved hand-authored Kalimari treatment. Its curve geometry
    # is kept separate from later maps so those revisions do not alter it.
    'kalimari_desert': dict(frame=(64,96), strokes=[(2.2,
        'M 30 26 L 30 23 C 30 16 36 10 43 10 C 50 10 55 16 55 23 '
        'L 55 40 C 55 45 54 47 49 49 C 45 51 45 54 45 59 '
        'L 45 70 C 45 80 38 85 27 85 L 20 85 C 8 85 6 80 9 72 '
        'C 10 68 14 64 19 60 L 27 52 C 30 49 30 47 30 43 L 30 26 Z'), (2.2,
        'M 17 40 C 17 34 23 27 29 27 C 37 27 44 33 44 40 '
        'L 44 70 C 44 77 37 82 30 82 C 23 82 17 76 17 69 L 17 40 Z')]),
    'sherbet_land': dict(frame=(64,64), strokes=[(2.4,
        'M 15 35 L 16 24 C 17 21 21 20 24 17 L 30 10 '
        'L 38 9 C 41 9 41 1 44 1 C 46 1 45 8 48 10 '
        'L 53 13 L 53 17 C 52 20 59 22 59 24 C 60 26 53 26 52 30 '
        'C 49 35 53 38 51 42 L 48 48 C 45 52 42 48 39 44 '
        'L 33 38 C 29 34 26 34 23 37 L 17 41 C 14 44 14 48 18 49 '
        'L 21 51 C 27 53 17 60 13 60 C 9 60 5 57 5 53 '
        'C 4 49 5 47 8 43 L 13 38 L 15 35 Z')], fills=[
        'M 17 20 L 25 14 L 31 8 L 39 6 L 42 2 L 43 8 L 39 12 '
        'L 33 13 L 33 18 L 30 20 L 25 20 L 24 23 L 19 25 L 16 31 Z',
        'M 28 34 C 33 34 37 40 41 44 L 45 48 L 49 44 L 49 49 '
        'C 46 54 41 51 38 47 L 33 42 L 26 39 L 22 39 Z',
        'M 15 48 C 18 48 23 50 23 54 C 23 58 17 62 12 62 '
        'L 6 58 L 7 56 L 13 57 L 18 55 L 19 53 L 15 52 Z',
        'M 43 22 C 45 20 49 21 50 23 C 49 26 45 26 43 24 Z'], cuts=[
        'M 25 15 L 27 12 L 31 12 L 31 15 L 28 18 L 25 18 Z']),
    'rainbow_road': dict(frame=(64,96), strokes=[(1.8,
        'M 40 5 C 40 0 43 0 47 0 C 51 0 53 3 52 7 '
        'C 50 14 44 17 38 20 C 31 24 28 27 27 32 L 25 41 '
        'C 24 45 22 48 22 51 L 22 59 C 22 64 18 66 14 69 '
        'L 7 74 C 2 78 5 84 10 83 L 24 75 C 26 73 28 73 31 73 '
        'L 51 73 C 57 73 61 79 61 83 C 61 89 56 91 51 90 '
        'C 45 89 43 85 39 83 C 33 80 29 84 29 89 C 29 94 35 97 39 93 '
        'C 43 89 41 84 42 78 L 42 69 C 42 66 40 63 40 59 L 40 5 Z'), (1.7,
        'M 23 45 C 25 42 29 45 30 48 C 32 52 27 55 24 53 C 21 52 22 48 23 45 Z')]),
    'wario_stadium': dict(frame=(64,64), strokes=[(3.6,
        'M 11 23 L 11 13 C 11 6 17 3 23 3 L 44 3 C 50 3 53 8 53 13 '
        'L 53 49 C 53 57 47 60 40 60 L 22 60 C 15 60 11 57 11 51 '
        'L 11 46 C 11 43 14 43 18 43 L 31 43'), (3.5,
        'M 11 23 L 23 23 C 26 23 27 25 27 28 L 27 34 '
        'C 27 38 20 38 20 34 L 20 29 C 20 27 18 26 15 26 L 11 25'), (3.4,
        'M 34 59 L 34 13 C 34 8 40 8 40 13 L 40 32 '
        'C 40 37 44 37 48 36 L 48 10'), (2.2,
        'M 36 43 L 52 43')], fills=[
        'M 48 9 L 53 13 L 53 46 L 49 46 Z',
        'M 38 56 L 49 52 L 49 58 L 43 61 L 36 61 Z']),
    'block_fort': dict(frame=(64,64), fills=['M 7 7 L 57 7 L 57 57 L 7 57 Z']),
    'skyscraper': dict(frame=(64,64), fills=[
        'M 9 26 L 26 9 L 38 9 L 55 26 L 55 38 L 38 55 L 26 55 L 9 38 Z'], cuts=[
        'M 15 27 L 27 15 L 30 15 L 30 18 L 19 29 L 15 29 Z',
        'M 34 15 L 37 15 L 49 27 L 49 30 L 45 30 L 34 19 Z',
        'M 15 34 L 19 34 L 30 45 L 30 49 L 27 49 L 15 37 Z',
        'M 34 45 L 45 34 L 49 34 L 49 37 L 37 49 L 34 49 Z',
        'M 30 32 C 30 29 34 29 34 32 C 34 35 30 35 30 32 Z']),
    'double_deck': dict(frame=(64,64), fills=['M 10 10 L 54 10 L 54 54 L 10 54 Z'], cuts=[
        'M 16 16 L 20 16 L 20 29 L 16 29 Z',
        'M 26 16 L 28 16 L 28 21 L 26 21 Z',
        'M 34 16 L 48 16 L 48 21 L 34 21 Z',
        'M 27 27 L 37 27 L 37 38 L 27 38 Z',
        'M 43 27 L 48 27 L 48 29 L 43 29 Z',
        'M 43 36 L 48 36 L 48 49 L 43 49 Z',
        'M 16 36 L 20 36 L 20 38 L 16 38 Z',
        'M 16 44 L 29 44 L 29 49 L 16 49 Z',
        'M 35 45 L 37 45 L 37 49 L 35 49 Z']),
    'dks_jungle_parkway': dict(frame=(64,64), strokes=[(2.4,
        'M 63 19 C 55 17 48 17 41 18 C 37 19 36 17 36 14 '
        'L 36 10 C 36 6 33 7 29 7 L 23 7 C 15 7 21 18 21 22 '
        'C 21 28 17 27 14 22 C 11 17 8 15 5 19 '
        'C 1 23 1 29 2 34 C 3 38 7 39 12 40 L 21 43 '
        'C 27 46 21 50 18 54 C 14 59 17 62 21 61 '
        'C 27 60 29 55 29 51 L 29 42 C 29 38 33 37 37 35 '
        'C 43 31 48 29 53 29 C 58 29 61 31 61 34 '
        'C 61 38 57 40 54 39 C 51 38 54 35 54 32')]),
    'big_donut': dict(frame=(64,64), fills=[
        'M 32 6 C 47 6 58 18 58 32 C 58 47 46 58 32 58 '
        'C 17 58 6 46 6 32 C 6 18 17 6 32 6 Z'], cuts=[
        'M 32 18 C 40 18 46 24 46 32 C 46 40 40 46 32 46 '
        'C 24 46 18 40 18 32 C 18 24 24 18 32 18 Z',
        'M 14 17 L 19 13 L 21 15 L 16 19 Z',
        'M 43 14 L 49 19 L 47 21 L 41 16 Z',
        'M 15 42 L 20 47 L 18 49 L 13 44 Z',
        'M 45 44 L 50 41 L 51 43 L 47 48 L 44 49 L 43 47 Z']),
}


def subpaths(path):
    """Flatten an authored M/L/C/Z path without importing an SVG engine."""
    tokens=re.findall(r'[MLCZ]|-?\d+(?:\.\d+)?',path)
    i=0;points=[];last=(0,0);start=last
    while i<len(tokens):
        op=tokens[i];i+=1
        if op=='Z':
            points.append(start);last=start;continue
        count={'M':2,'L':2,'C':6}[op]
        v=list(map(float,tokens[i:i+count]));assert len(v)==count;i+=count
        end=tuple(v[-2:])
        if op=='M':
            if points:yield points
            points=[end];start=end
        elif op=='L':points.append(end)
        else:
            for step in range(1,65):
                t=step/64;s=1-t
                points.append(tuple(s*s*s*last[c]+3*s*s*t*v[c]+3*s*t*t*v[c+2]+t*t*t*end[c] for c in (0,1)))
        last=end
    if points:yield points


def rasterize(course,width,height):
    ss=4;fw,fh=course['frame'];scale=min(width/fw,height/fh)
    mask=Image.new('L',(width*ss,height*ss));draw=ImageDraw.Draw(mask)
    def points(path):
        for part in subpaths(path):yield [(x*width/fw*ss,y*height/fh*ss) for x,y in part]
    for stroke,path in course.get('strokes',[]):
        radius=max(1,round(stroke*scale*ss))/2
        for part in points(path):
            draw.line(part,fill=255,width=round(radius*2),joint='curve')
            # Union the discs as well as the segment rectangles. Pillow's
            # short-segment joins alone can leave tiny interior pinholes at
            # a sampled cubic; a solid brush cannot have those holes.
            for x,y in part:draw.ellipse((x-radius,y-radius,x+radius,y+radius),fill=255)
    for paths,value in ((course.get('fills',[]),255),(course.get('cuts',[]),0)):
        for path in paths:
            for part in points(path):draw.polygon(part,fill=value)
    # Match the exact LA4 upload, so a preview shows the same edge coverage as
    # the console. There is no later GPU blur or threshold reconstruction.
    return mask.resize((width,height),Image.Resampling.BOX).point(lambda a:round(a/17)*17)


def encode(mask):
    samples=mask.get_flattened_data() if hasattr(mask,'get_flattened_data') else mask.getdata()
    values=[a//17 for a in samples];runs=[];last=values[0];count=0
    for value in values:
        if value!=last or count==4095:
            runs.append(count<<4|last);last=value;count=0
        count+=1
    runs.append(count<<4|last)
    return runs


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--preview',type=Path)
    p.add_argument('--check',action='store_true',help='Verify the generated header without writing it.')
    a=p.parse_args()
    lines=['// Generated by assets/native-ui/generate_maps.py. Hand-authored geometry.',
        '#pragma once','#include "native_mask_3ds.hpp"','namespace mk64_3ds::native_art {']
    catalog=[];previews=[];encoded=0
    for name,course in MAPS.items():
        symbols=[];fw,fh=course['frame']
        for i,(aw,ah) in enumerate(((149,184),(110,140),(68,176))):
            scale=min(aw/fw,ah/fh);w,h=round(fw*scale),round(fh*scale)
            mask=rasterize(course,w,h);runs=encode(mask);encoded+=len(runs)*2
            symbol='k'+''.join(part.title() for part in name.split('_'))+f'Map{i}'
            lines.append(f'inline constexpr uint16_t {symbol}Runs[] = {{')
            for offset in range(0,len(runs),20):lines.append('    '+','.join(f'0x{v:04x}' for v in runs[offset:offset+20])+',')
            lines+=['};',f'inline constexpr MaskSprite {symbol}{{{w},{h},{symbol}Runs,sizeof({symbol}Runs)/sizeof(uint16_t)}};']
            symbols.append(symbol)
            if a.preview:
                a.preview.mkdir(parents=True,exist_ok=True)
                im=Image.new('RGBA',mask.size,'white');im.putalpha(mask);im.save(a.preview/f'{name}-{i}.png')
                if i==0:previews.append((name,im))
        catalog.append('{"minimap_'+name+'",{'+','.join(symbols)+'}}')
    lines+=['struct CourseMaps { const char* resource; MaskSprite sizes[3]; };',
        'inline constexpr CourseMaps kCourseMaps[] = {',*('    '+v+',' for v in catalog),'};',
        '} // namespace mk64_3ds::native_art']
    target=Path(__file__).resolve().parents[2]/'source/art/native_map_art_3ds.hpp';text='\n'.join(lines)+'\n'
    if a.check:
        if not target.exists() or target.read_text()!=text:p.error('Generated course art is stale; regenerate it.')
    elif not target.exists() or target.read_text()!=text:target.write_text(text)
    if a.preview:
        sheet=Image.new('RGB',(850,880),(22,22,22));draw=ImageDraw.Draw(sheet)
        for i,(name,im) in enumerate(previews):
            x=i%5*170;y=i//5*220;draw.text((x+4,y+4),name,fill='white')
            sheet.paste(im,(x+(170-im.width)//2,y+24),im)
        sheet.save(a.preview/'hand-authored-maps.png')
    print(f'{"Verified" if a.check else "Generated"} {len(MAPS)*3} native map canvases; {encoded} bytes of RLE coverage.')
if __name__=='__main__':main()
