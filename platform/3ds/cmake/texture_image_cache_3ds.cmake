# Texture-image command cache. Every course sub-display list names its texture
# by path or archive hash, and each frame resolved it through the full resource
# lookup (name normalization, owner acquire/release, transient-map search).
# Remember a weak reference per name pointer or hash instead: while any owner
# (the texture cache, the loaded tile) keeps the texture alive, reuse it
# directly; otherwise take the original path. Applied after every other
# interpreter patch; each anchor must match exactly once.
macro(mk64_timg_replace old_text new_text description)
    string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "${old_text}" mk64_timg_first)
    string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "${old_text}" mk64_timg_last REVERSE)
    if(mk64_timg_first EQUAL -1 OR NOT mk64_timg_first EQUAL mk64_timg_last)
        message(FATAL_ERROR "Texture image cache integration changed: ${description}")
    endif()
    string(REPLACE "${old_text}" "${new_text}"
        SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
endmacro()

mk64_timg_replace(
    "#include \"performance_trace_3ds.hpp\""
    "#include \"performance_trace_3ds.hpp\"\n#include \"settings_3ds.h\""
    "texture policy declaration")

mk64_timg_replace(
    "bool gfx_set_timg_handler_rdp(F3DGfx** cmd0) {"
    [=[extern "C" uint32_t Mk64Resource3DSGeneration(void) __attribute__((weak));
extern "C" Mk64UiTextures3DS Mk64Settings3DSGetUiTextures(void) __attribute__((weak));
namespace {
struct Mk64TextureImageEntry3DS {
    uint64_t key = 0;
    uint32_t check = 0;
    uint32_t generation = 0;
    int mode = 0;
    std::weak_ptr<Fast::Texture> texture;
};
std::array<Mk64TextureImageEntry3DS, 512> sMk64TextureImages3DS;
// Normal, archive-hash and filepath requests, then hits. No per-call clocks.
uint64_t sMk64TextureImageRequests3DS[3] = {};
uint64_t sMk64TextureImageHits3DS[3] = {};

int Mk64TextureImageMode3DS() {
    return Mk64Settings3DSGetUiTextures != nullptr ? Mk64Settings3DSGetUiTextures() : 0;
}

uint32_t Mk64TextureImageGeneration3DS() {
    return Mk64Resource3DSGeneration != nullptr ? Mk64Resource3DSGeneration() : 0;
}

Mk64TextureImageEntry3DS& Mk64TextureImageSlot3DS(uint64_t key) {
    uint64_t mixed = key * 0x9E3779B97F4A7C15ULL;
    return sMk64TextureImages3DS[(mixed >> 32) % sMk64TextureImages3DS.size()];
}

std::shared_ptr<Fast::Texture> Mk64TextureImageHit3DS(const Mk64TextureImageEntry3DS& entry, uint64_t key,
                                                      uint32_t check) {
    if (entry.key != key || entry.check != check || entry.generation != Mk64TextureImageGeneration3DS() ||
        entry.mode != Mk64TextureImageMode3DS())
        return nullptr;
    auto texture = entry.texture.lock();
    return texture != nullptr && texture->ImageData != nullptr ? texture : nullptr;
}

void Mk64TextureImageStore3DS(Mk64TextureImageEntry3DS& entry, uint64_t key, uint32_t check,
                              const std::shared_ptr<Fast::Texture>& texture) {
    if (texture == nullptr || texture->ImageData == nullptr) return;
    entry.key = key;
    entry.check = check;
    entry.generation = Mk64TextureImageGeneration3DS();
    entry.mode = Mk64TextureImageMode3DS();
    entry.texture = texture;
}

// A name pointer only selects a cache slot. Check the complete normalized
// name held by the live resource, so reused buffers and hash collisions cannot
// return a different texture. This also avoids hashing every name byte.
std::shared_ptr<Fast::Texture> Mk64TextureByName3DS(const char* fileName, size_t route = 2) {
    ++sMk64TextureImageRequests3DS[route];
    if (fileName == nullptr || *fileName == 0) return nullptr;
    const uint32_t check = 0;
    const uint64_t key = reinterpret_cast<uintptr_t>(fileName);
    auto& entry = Mk64TextureImageSlot3DS(key);
    if (auto texture = Mk64TextureImageHit3DS(entry, key, check)) {
        const char* normalized = std::strncmp(fileName, "__OTR__", 7) == 0 ? fileName + 7 : fileName;
        if (texture->GetInitData()->Path == normalized) {
            ++sMk64TextureImageHits3DS[route];
            return texture;
        }
    }
    auto texture = std::static_pointer_cast<Fast::Texture>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(fileName));
    Mk64TextureImageStore3DS(entry, key, check, texture);
    return texture;
}

std::shared_ptr<Fast::Texture> Mk64TextureByHash3DS(uint64_t hash, const char** fileName) {
    ++sMk64TextureImageRequests3DS[1];
    constexpr uint32_t kHashKey = 0xFFFFFFFFU;
    auto& entry = Mk64TextureImageSlot3DS(hash);
    if (auto texture = Mk64TextureImageHit3DS(entry, hash, kHashKey)) {
        ++sMk64TextureImageHits3DS[1];
        *fileName = texture->GetInitData()->Path.c_str();
        return texture;
    }
    *fileName = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->HashToCString(hash);
    if (*fileName == nullptr) return nullptr;
    auto texture = std::static_pointer_cast<Fast::Texture>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(*fileName));
    Mk64TextureImageStore3DS(entry, hash, kHashKey, texture);
    return texture;
}
} // namespace

extern "C" void Mk64TextureImages3DSStats(uint64_t out[6]) {
    for (size_t route = 0; route < 3; ++route) {
        out[route * 2] = sMk64TextureImageRequests3DS[route];
        out[route * 2 + 1] = sMk64TextureImageHits3DS[route];
    }
}

bool gfx_set_timg_handler_rdp(F3DGfx** cmd0) {]=]
    "cache helpers")

# Only replace the lookup behind the ordinary handler's existing signature
# check. SegAddr, raw pointers, missing resources, TLUTs and metadata stay live.
mk64_timg_replace(
    [=[            std::shared_ptr<Fast::Texture> tex = std::static_pointer_cast<Fast::Texture>(
                Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(imgData));]=]
    [=[            std::shared_ptr<Fast::Texture> tex = Mk64TextureByName3DS(imgData, 0);]=]
    "ordinary named texture")

mk64_timg_replace(
    [=[    const char* fileName =
        Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->HashToCString(hash);
    const uint32_t fmt = C0(21, 3);]=]
    [=[    const char* fileName = nullptr;
    std::shared_ptr<Fast::Texture> cachedTexture3DS = Mk64TextureByHash3DS(hash, &fileName);
    const uint32_t fmt = C0(21, 3);]=]
    "hash lookup")

mk64_timg_replace(
    [=[    std::shared_ptr<Fast::Texture> texture = std::static_pointer_cast<Fast::Texture>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(fileName));
    if (texture == nullptr || texture->ImageData == nullptr) {
        gfx->GfxDpSetTextureImage(fmt, size, width, fileName, 0,]=]
    [=[    std::shared_ptr<Fast::Texture> texture = std::move(cachedTexture3DS);
    if (texture == nullptr || texture->ImageData == nullptr) {
        gfx->GfxDpSetTextureImage(fmt, size, width, fileName, 0,]=]
    "hash texture")

mk64_timg_replace(
    [=[    std::shared_ptr<Fast::Texture> texture = std::static_pointer_cast<Fast::Texture>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(fileName));
    if (texture != nullptr && texture->ImageData != nullptr) {
        Interpreter* gfx = mInstance;
        texFlags = texture->Flags;]=]
    [=[    std::shared_ptr<Fast::Texture> texture = Mk64TextureByName3DS(fileName);
    if (texture != nullptr && texture->ImageData != nullptr) {
        Interpreter* gfx = mInstance;
        texFlags = texture->Flags;]=]
    "path texture")
