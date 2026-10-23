#!/usr/bin/env python3
"""Prepare the original UI font locally from the user's validated O2R archive."""
import argparse
import json
from pathlib import Path
import zipfile
from core import verify_archive

NAMES = ['font_letter_'+c for c in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ']
NAMES += ['font_number_'+n for n in ('zero','one','two','three','four','five','six','seven','eight','nine')]
NAMES += ['7F1534','font_minus','7F16D4','font_exclamation_mark','font_plus','7F17A4']
CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-?!+\''
ADVANCES = [12,13,11,11,10,11,11,13,7,10,12,10,18,13,12,12,12,12,11,13,12,12,18,13,12,12,10,8,11,12,12,13,10,11,10,10,6,10,10,10,10,6]


def prepare(archive):
    verify_archive(archive)
    glyphs={}
    with zipfile.ZipFile(archive) as z:
        for char,name,advance in zip(CHARS,NAMES,ADVANCES,strict=True):
            raw=z.read('textures/texture_data_2/'+name)
            if len(raw)!=288:
                raise ValueError('Unexpected font texture size: '+name)
            glyphs[char]={'advance':advance,'i4':raw[80:].hex()}
    target=Path(__file__).parent/'runtime/ui/font-i4.json'
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(json.dumps(glyphs))
    return target


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive',type=Path)
    print(prepare(parser.parse_args().archive))
