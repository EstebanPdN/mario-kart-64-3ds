#!/usr/bin/env python3
"""Compile the production release helper against a reentrant eviction callback."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline', action='store_true')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
port = Path(__file__).resolve().parents[1]
text = (port/'cmake/interpreter_patches_3ds.cmake').read_text()
begin = text.index('static void Mk64ReleaseTextureOwner3DS(')
end = text.index('static void Mk64ResetTextureOwners3DS()', begin)
functions = text[begin:end]
if a.baseline:
    functions = functions.replace('        auto released = std::move(sMk64TextureOwners3DS[textureId]);\n', '')
test = r'''
#include <array>
#include <memory>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <utility>
struct Owners {
    std::shared_ptr<uint8_t> image;
    std::array<std::shared_ptr<uint8_t>, 2> palettes;
    const uint8_t* imageBase = nullptr;
    std::array<const uint8_t*, 2> paletteBases{};
};
std::array<Owners, 2> sMk64TextureOwners3DS;
std::array<uint32_t, 2> sMk64TextureFrameUses3DS{};
FUNCTIONS
int main() {
    for (unsigned kind=0; kind<3; ++kind) {
        bool replacementNodePresent=true;
        unsigned callbacks=0;
        auto* bytes=new uint8_t(42);
        std::shared_ptr<uint8_t> resource(bytes,[&](uint8_t* p) {
            ++callbacks;
            // Mirrors TextureCacheDelete after the LRU node has been re-keyed.
            if (Mk64TextureOwnerReferences3DS(1,p)) replacementNodePresent=false;
            Mk64ReleaseTextureOwner3DS(1); // callback may re-enter the release
            delete p;
        });
        if (!kind) {
            sMk64TextureOwners3DS[1].image=std::move(resource);
            sMk64TextureOwners3DS[1].imageBase=bytes;
        } else {
            sMk64TextureOwners3DS[1].palettes[kind-1]=std::move(resource);
            sMk64TextureOwners3DS[1].paletteBases[kind-1]=bytes;
        }
        sMk64TextureFrameUses3DS[1]=7;
        Mk64ReleaseTextureOwner3DS(1);
        assert(replacementNodePresent);
        assert(callbacks==1);
        assert(!Mk64TextureOwnerReferences3DS(1,bytes));
        assert(sMk64TextureFrameUses3DS[1]==0);
    }
    std::puts("PASS: reentrant image/palette release cannot erase a recycled texture node.");
}
'''.replace('FUNCTIONS', functions)
source=a.output/'texture-owner-release.cpp'
source.write_text(test)
binary=a.output/'texture-owner-release'
subprocess.run(['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined',
                '-fno-omit-frame-pointer','-Wall','-Wextra','-Werror',str(source),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
