#!/usr/bin/env python3
"""Assemble offline Windows/Linux bundles from verified native build inputs."""
import argparse
from pathlib import Path
import shutil
import tarfile
import zipfile

HERE=Path(__file__).resolve().parent


def assemble(platform, python_tar, wheels, engine, launcher, destination, system_libs=None):
    destination=Path(destination)
    destination.mkdir(parents=True,exist_ok=False)
    with tarfile.open(python_tar) as archive:
        # Terminfo's case-only alias names collide on macOS volumes when
        # assembling a Linux package. This Qt app does not use terminal data.
        members=(member for member in archive if not member.name.startswith('python/share/terminfo/'))
        archive.extractall(destination,members=members,filter='data')
    python=destination/'python'
    site=python/('Lib/site-packages' if platform=='windows' else 'lib/python3.14/site-packages')
    site.mkdir(parents=True,exist_ok=True)
    for wheel in Path(wheels).glob('*.whl'):
        with zipfile.ZipFile(wheel) as archive:
            archive.extractall(site)
    app=destination/'app';app.mkdir()
    for name in ('app.py','core.py','pixel_ui.py','game_font.py','CREDITS.txt'):
        shutil.copy2(HERE/name,app/name)
    for name in ('assets','licenses'):
        shutil.copytree(HERE/name,app/name)
    for name in ('metadata',):
        shutil.copytree(HERE/'runtime'/name,app/'runtime'/name)
    binary=app/'runtime/bin'/('mk64-extract.exe' if platform=='windows' else 'mk64-extract')
    binary.parent.mkdir();shutil.copy2(engine,binary);binary.chmod(0o755)
    if system_libs:
        shutil.copytree(system_libs,app/'runtime/linux-libs',symlinks=False)
    entry=destination/('MK64 Extractor.exe' if platform=='windows' else 'MK64 Extractor')
    shutil.copy2(launcher,entry);entry.chmod(0o755)
    shutil.copy2(HERE/'README.md',destination/'README.txt')
    shutil.copy2(HERE/'CREDITS.txt',destination/'CREDITS.txt')
    # Retain Python/Qt license trees and runtime support files; remove caches.
    for cache in destination.rglob('__pycache__'):
        shutil.rmtree(cache)
    return destination


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform',choices=('windows','linux'),required=True)
    parser.add_argument('--python-tar',type=Path,required=True)
    parser.add_argument('--wheels',type=Path,required=True)
    parser.add_argument('--engine',type=Path,required=True)
    parser.add_argument('--launcher',type=Path,required=True)
    parser.add_argument('--destination',type=Path,required=True)
    parser.add_argument('--system-libs',type=Path)
    args=parser.parse_args()
    print(assemble(**vars(args)))
