#!/usr/bin/env python3
"""Check maintained documentation, Python syntax and generated port artwork.

This is a source consistency check, not a runtime or physical-hardware test.
It deliberately excludes pinned third-party trees and private runtime data.
"""
import argparse
import ast
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artwork',action='store_true',help='Also verify the Pillow-generated course masks.')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[3]
    port=root/'platform/3ds'
    documents=[root/name for name in ('README.md','CHANGELOG.md','CONTRIBUTING.md')]
    documents+=sorted((port/'docs').glob('*.md'))
    documents.append(port/'update/README.md')
    errors=[];links=0
    for document in documents:
        if not document.is_file():
            errors.append('Missing document: '+str(document.relative_to(root)));continue
        for target in re.findall(r'\[[^\]]*\]\(([^)]+)\)',document.read_text()):
            target=target.strip().strip('<>')
            if urlsplit(target).scheme or target.startswith('#'):continue
            path=unquote(target.split('#',1)[0])
            resolved=(document.parent/path).resolve()
            if path and (not resolved.exists() or (resolved!=root and root not in resolved.parents)):
                errors.append(str(document.relative_to(root))+': broken or external local link '+target)
            links+=1
    scripts=sorted(port.rglob('*.py'))
    for script in scripts:
        ast.parse(script.read_text(),filename=str(script))
    for relative in ('menu-hints/generate.py','menu-hints/generate_icons.py','native-ui/generate.py'):
        subprocess.run([sys.executable,str(port/'assets'/relative),'--check'],check=True)
    if args.artwork:
        subprocess.run([sys.executable,str(port/'assets/native-ui/generate_maps.py'),'--check'],check=True)
    if errors:
        raise SystemExit('\n'.join(errors))
    print(f'PASS: {len(documents)} documents, {links} local links, {len(scripts)} Python scripts and generated-art consistency.')


if __name__=='__main__':main()
