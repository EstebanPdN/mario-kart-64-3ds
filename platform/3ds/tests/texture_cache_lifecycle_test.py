#!/usr/bin/env python3
"""Run actual patched Fast3D cache functions with reentrant resource deletion."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline-clear', action='store_true')
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
port = Path(__file__).resolve().parents[1]
upstream = port.parents[1] / 'third_party/SpaghettiKart/libultraship'
patch = (port/'cmake/interpreter_patches_3ds.cmake').read_text()
patch = patch[patch.index('        set(SPAGHETTIKART_3DS_TEXTURE_CACHE_ORIGINAL'):patch.index('        set(SPAGHETTIKART_3DS_MATRIX_LOOKUP_ORIGINAL')]
driver = out/'apply-cache.cmake'
driver.write_text(f'file(READ "{upstream}/src/fast/interpreter.cpp" SPAGHETTIKART_3DS_INTERPRETER_TEXT)\n' + patch +
                  f'\nfile(WRITE "{out}/cache.cpp" "${{SPAGHETTIKART_3DS_INTERPRETER_TEXT}}")\n')
subprocess.run(['cmake', '-P', str(driver)], check=True)
generated = (out/'cache.cpp').read_text()
helpers = generated[generated.index('extern "C" size_t Mk64Graphics3DSTextureCacheCapacity'):generated.index('void Interpreter::TextureCacheClear()')]
helpers = helpers[:helpers.rfind('#endif')]
def function(signature):
    start = generated.index(signature)
    body = generated.index('{', start)
    depth = 1
    end = body+1
    while depth:
        depth += (generated[end] == '{') - (generated[end] == '}')
        end += 1
    return generated[start:end]
functions = '\n'.join(function(s) for s in ['void Interpreter::TextureCacheClear()',
    'bool Interpreter::TextureCacheLookup(', 'void Interpreter::TextureCacheDelete('])
if a.baseline_clear:
    functions = functions.replace('mTextureCache.free_texture_ids.push_back(entry.second.texture_id);',
        'mTextureCache.free_texture_ids.push_back(entry.second.texture_id);\nMk64ReleaseTextureOwner3DS(entry.second.texture_id);')
header = (upstream/'include/fast/interpreter.h').read_text()
types = header[header.index('struct TextureCacheKey {'):header.index('struct RGBA {')]
types += header[header.index('struct GfxTextureCache {'):header.index('struct ColorCombiner {')]
fixture = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <list>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>
#include "fast/resource/type/Texture.h"
#include "texture_pressure_3ds.hpp"
#define TEXTURE_CACHE_MAX_SIZE 512
#define SHADER_MAX_TEXTURES 2
TYPES
struct RDP {
    struct Loaded {
        struct Meta { std::shared_ptr<Fast::Texture> resource; } raw_tex_metadata;
        const uint8_t* addr = nullptr;
        uint32_t orig_size_bytes = 0, size_bytes = 0;
    } loaded_texture[2];
    const uint8_t* palette_dram_addr[2]{};
};
struct Renderer {
    uint32_t next = 0;
    uint32_t NewTexture() { return ++next; }
    void SelectTexture(int, uint32_t) {}
    void SetSamplerParameters(int, bool, int, int) {}
};
struct Interpreter {
    GfxTextureCache mTextureCache;
    struct State { TextureCacheNode* mTextures[2]{}; } mRenderingState;
    std::unordered_map<int,int> mMaskedTextures;
    RDP* mRdp;
    Renderer* mRapi;
    void Flush() {}
    bool TextureCacheLookup(int, const TextureCacheKey&);
    void TextureCacheClear();
    void TextureCacheDelete(const uint8_t*);
};
HELPERS
extern "C" size_t Mk64Graphics3DSTextureCacheCapacity() { return 2; }
extern "C" size_t Mk64Graphics3DSTextureCacheSpillCapacity() { return 4; }
extern "C" bool Mk64Graphics3DSFenceTextureReuseBatch() { return true; }
FUNCTIONS
int main() {
    RDP rdp{}; Renderer renderer{}; Interpreter interpreter{};
    interpreter.mRdp = &rdp; interpreter.mRapi = &renderer;
    interpreter.TextureCacheClear();
    unsigned freed = 0;
    for (unsigned frame = 0; frame < 1000; ++frame) {
        Mk64BeginTextureFrame3DS();
        auto init = std::make_shared<Ship::ResourceInitData>(); init->Path = "synthetic";
        auto image = std::make_shared<Fast::Texture>(init);
        image->mImageBuffer = std::shared_ptr<std::vector<char>>(new std::vector<char>(16, frame),
            [&](auto* data) {
                interpreter.TextureCacheDelete(reinterpret_cast<const uint8_t*>(data->data()));
                ++freed; delete data;
            });
        image->ImageData = reinterpret_cast<uint8_t*>(image->mImageBuffer->data());
        image->ImageDataSize = 16;
        rdp.loaded_texture[0] = {{image}, image->ImageData, 16, 16};
        TextureCacheKey key{image->ImageData, {nullptr,nullptr}, 0,0,0,16};
        assert(!interpreter.TextureCacheLookup(0, key));
        assert(interpreter.TextureCacheLookup(0, key));
        assert(interpreter.mRenderingState.mTextures[0]->first.texture_addr == image->ImageData);
        assert(interpreter.mTextureCache.map.size() == std::min(frame+1, 2U));
        assert(interpreter.mTextureCache.free_texture_ids.empty());
        assert(sMk64TextureOwners3DS[interpreter.mRenderingState.mTextures[0]->second.texture_id].image == image);
        assert(freed == (frame > 1 ? frame-1 : 0));
    }
    rdp = {};
    interpreter.TextureCacheClear();
    assert(freed == 1000 && interpreter.mTextureCache.map.empty());
    assert(interpreter.mTextureCache.lru.empty());
    assert(renderer.next == 2 && interpreter.mTextureCache.free_texture_ids.size() == 2);
    std::set<uint32_t> ids(interpreter.mTextureCache.free_texture_ids.begin(), interpreter.mTextureCache.free_texture_ids.end());
    assert(ids.size() == 2);
    for (const auto& owner : sMk64TextureOwners3DS) assert(!owner.image && !owner.imageBase);
    std::puts("PASS: 1000 real cache insert/hit/recycle operations, reentrant deletion, scene clear and unique free GPU IDs.");
}
'''.replace('TYPES', types).replace('HELPERS', helpers).replace('FUNCTIONS', functions)
(out/'cache-test.cpp').write_text(fixture)
subprocess.run(['clang++','-std=c++20','-D__3DS__','-O1','-g','-fsanitize=address,undefined',
    '-fno-omit-frame-pointer','-I',str(port/'probe_stubs'),'-I',str(port/'include'),'-I',str(upstream/'include'),
    str(out/'cache-test.cpp'),str(upstream/'src/fast/resource/type/Texture.cpp'),'-o',str(out/'cache-test')],check=True)
subprocess.run([str(out/'cache-test')],check=True)
