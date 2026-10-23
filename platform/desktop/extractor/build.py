#!/usr/bin/env python3
"""Build the host extraction engine, then optionally a standalone desktop app."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
PORT = HERE.parents[2]


def prepare(build):
    runtime = HERE / 'runtime'
    binary = build / ('mk64-extract.exe' if os.name == 'nt' else 'mk64-extract')
    if not binary.exists():
        binary = build / 'Release' / binary.name
    (runtime / 'bin').mkdir(parents=True, exist_ok=True)
    shutil.copy2(binary, runtime / 'bin' / binary.name)
    upstream = PORT / 'third_party' / 'SpaghettiKart'
    metadata = runtime / 'metadata'
    metadata.mkdir(parents=True, exist_ok=True)
    shutil.copy2(upstream / 'config.yml', metadata / 'config.yml')
    for name in ('yamls', 'meta'):
        target = metadata / name
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(upstream / name, target)
    assets = HERE / 'assets'
    assets.mkdir(exist_ok=True)
    # Artwork is checked in beside the app; do not replace the transparent
    # desktop logo with the opaque HOME-menu banner during packaging.
    for name in ('logo-hd.png','heading-pack.png','actions-pack.png','background-loading.png',
                 'font-hd.png','font-hd.json','icon.png'):
        if not (assets / name).is_file():
            raise FileNotFoundError('Desktop artwork is missing: '+name)
    licenses = HERE / 'licenses'
    licenses.mkdir(exist_ok=True)
    shutil.copy2(upstream / 'torch/LICENSE', licenses / 'Torch-LICENSE.txt')
    for label, pattern in (('yaml-cpp', 'LICENSE'), ('spdlog', 'LICENSE'), ('tinyxml2', 'LICENSE.txt')):
        candidates = list((build / '_deps').glob(f'{label}-src/{pattern}'))
        if candidates:
            shutil.copy2(candidates[0], licenses / f'{label}-LICENSE.txt')
    print('Extraction engine and metadata are ready:', runtime)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=HERE / 'build/native')
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--package', action='store_true')
    parser.add_argument('--cmake-arg', action='append', default=[])
    args = parser.parse_args()
    build = args.build_dir.resolve()
    if not args.prepare_only:
        command = ['cmake', '-S', str(HERE / 'native'), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release']
        if sys.platform == 'darwin':
            command.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0')
        subprocess.run(command + args.cmake_arg, check=True)
        subprocess.run(['cmake', '--build', str(build), '--config', 'Release', '--parallel', '4'], check=True)
    prepare(build)
    if args.package:
        subprocess.run([sys.executable, '-m', 'PyInstaller', '--noconfirm', str(HERE / 'MK64 Extractor.spec')], cwd=HERE, check=True)


if __name__ == '__main__':
    main()
