        # ColorCombinerKey's defaulted comparison includes shader_id, although
        # upstream GfxSpTri1 only assigns combine_mode/options. Stack garbage
        # in that unused field creates distinct map entries for identical GPU
        # state, accumulating ordinary heap across frames and track resets.
        string(REPLACE "#include \"fast/types.h\""
            "#include \"fast/types.h\"\n#include \"combiner_cache_3ds.hpp\""
            SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    Flush();
    mPrevCombiner = mColorCombinerPool.insert(std::make_pair(key, ColorCombiner())).first;
    GenerateCC(&mPrevCombiner->second, key);]=]
            [=[    Flush();
    if (mColorCombinerPool.size() >= ::mk64_3ds::kCombinerCacheCapacity3DS) {
        // Flush consumes the old CPU mapping. GPU programs remain intact;
        // this neither removes geometry nor waits/reuploads any textures.
        mColorCombinerPool.clear();
        mPrevCombiner = mColorCombinerPool.end();
        sTriangleRun3DS.Invalidate(); // discard saved material pointers too
        ::mk64_3ds::RecordCombinerCacheLimitReset3DS();
    }
    mPrevCombiner = mColorCombinerPool.insert(std::make_pair(key, ColorCombiner())).first;
    ::mk64_3ds::PublishCombinerCacheSize3DS(mColorCombinerPool.size());
    GenerateCC(&mPrevCombiner->second, key);]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        # Fail closed if the upstream insertion point changes.
        if(NOT SPAGHETTIKART_3DS_INTERPRETER_TEXT MATCHES "RecordCombinerCacheLimitReset3DS")
            message(FATAL_ERROR "The Fast3D combiner cache patch points changed")
        endif()
        string(REPLACE
            "GfxExecStack g_exec_stack = {};"
            "extern \"C\" void Mk64Diagnostics3DSGfxWatchdog(const void*, uint32_t, uint32_t, size_t) __attribute__((weak));\n\nGfxExecStack g_exec_stack = {};"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        set(SPAGHETTIKART_3DS_CULL_INCLUDE_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "#include \"fast/types.h\""
            "#include \"fast/types.h\"\n#include \"fast3d_culling_3ds.hpp\"\n#include \"fast3d_math_3ds.hpp\"\n\nextern \"C\" bool Mk64FrameInterpolation3DSLookup(const Mtx*, MtxF*) __attribute__((weak));"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_CULL_INCLUDE_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D culling include point changed")
        endif()
        string(REPLACE "#include \"fast3d_culling_3ds.hpp\""
            "#include \"fast3d_culling_3ds.hpp\"\n#include \"render_policy_3ds.hpp\"\nextern \"C\" { extern float gMk64DistanceFar3DS; extern uint32_t gMk64DistanceCulled3DS; }"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        set(MK64_DISTANCE_PATCH_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "    struct LoadedVertex* v_arr[3] = { v1, v2, v3 };"
            "    struct LoadedVertex* v_arr[3] = { v1, v2, v3 };\n    if (::mk64_3ds::CullDistantTriangle(v1->w, v2->w, v3->w, gMk64DistanceFar3DS, is_rect, (mRsp->geometry_mode & G_ZBUFFER) != 0)) {\n        ++gMk64DistanceCulled3DS;\n        return;\n    }"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL MK64_DISTANCE_PATCH_ORIGINAL)
            message(FATAL_ERROR "The Fast3D distance-culling insertion point changed")
        endif()
        set(SPAGHETTIKART_3DS_CULL_HANDLER_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[bool gfx_cull_dl_handler_f3dex2(F3DGfx** cmd) {
    // TODO:
    return false;
}]=]
            [=[bool gfx_cull_dl_handler_f3dex2(F3DGfx** cmd) {
    Interpreter* gfx = mInstance.lock().get();
    ::mk64_3ds::Fast3DCullEncoding encoding3DS =
        ::mk64_3ds::Fast3DCullEncoding::F3DEX;
    if (ucode_handler_index == ucode_f3db || ucode_handler_index == ucode_f3d) {
        encoding3DS = ::mk64_3ds::Fast3DCullEncoding::F3D;
    } else if (ucode_handler_index == ucode_f3dex2) {
        encoding3DS = ::mk64_3ds::Fast3DCullEncoding::F3DEX2;
    }

    const auto range3DS = ::mk64_3ds::DecodeFast3DCullRange(
        (*cmd)->words.w0, (*cmd)->words.w1, encoding3DS, MAX_VERTICES);
    const bool shouldCull3DS = ::mk64_3ds::ShouldCullFast3DRange(
        range3DS, [gfx](std::size_t index3DS) {
            return gfx->mRsp->loaded_vertices[index3DS].clip_rej;
        });
    if (!shouldCull3DS) {
        return false;
    }

    gfx->mMarkerOn = false;
    g_exec_stack.ret();
    return true;
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_CULL_HANDLER_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D G_CULLDL handler changed")
        endif()
        set(SPAGHETTIKART_3DS_EXEC_STACK_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void GfxExecStack::start(F3DGfx* dlist) {
    while (!cmd_stack.empty())
        cmd_stack.pop();
    gfx_path.clear();
    cmd_stack.push(dlist);
    disp_stack.clear();
}

void GfxExecStack::stop() {
    while (!cmd_stack.empty())
        cmd_stack.pop();
    gfx_path.clear();
}

F3DGfx*& GfxExecStack::currCmd() {
    return cmd_stack.top();
}

void GfxExecStack::openDisp(const char* file, int line) {
    disp_stack.push_back({ file, line });
}
void GfxExecStack::closeDisp() {
    disp_stack.pop_back();
}
const std::vector<GfxExecStack::CodeDisp>& GfxExecStack::getDisp() const {
    return disp_stack;
}

void GfxExecStack::branch(F3DGfx* caller) {
    F3DGfx* old = cmd_stack.top();
    cmd_stack.pop();
    cmd_stack.push(nullptr);
    cmd_stack.push(old);

    gfx_path.push_back(caller);
}

void GfxExecStack::call(F3DGfx* caller, F3DGfx* callee) {
    cmd_stack.push(callee);
    gfx_path.push_back(caller);
}

F3DGfx* GfxExecStack::ret() {
    F3DGfx* cmd = cmd_stack.top();

    cmd_stack.pop();
    if (!gfx_path.empty()) {
        gfx_path.pop_back();
    }

    while (cmd_stack.size() > 0 && cmd_stack.top() == nullptr) {
        cmd_stack.pop();
        if (!gfx_path.empty()) {
            gfx_path.pop_back();
        }
    }
    return cmd;
}]=]
            [=[namespace {
constexpr size_t kMk64ExecStackCapacity3DS = 512;
std::array<F3DGfx*, kMk64ExecStackCapacity3DS> sMk64ExecStack3DS = {};
size_t sMk64ExecStackDepth3DS = 0;
F3DGfx* sMk64NullCommand3DS = nullptr;

bool Mk64ExecStackEmpty3DS() {
    return sMk64ExecStackDepth3DS == 0;
}

size_t Mk64ExecStackDepth3DS() {
    return sMk64ExecStackDepth3DS;
}

F3DGfx* Mk64ExecStackCurrent3DS() {
    return Mk64ExecStackEmpty3DS() ? nullptr
                                   : sMk64ExecStack3DS[sMk64ExecStackDepth3DS - 1];
}
}

void GfxExecStack::start(F3DGfx* dlist) {
    sMk64ExecStackDepth3DS = 0;
    gfx_path.clear();
    disp_stack.clear();
    if (gfx_path.capacity() < kMk64ExecStackCapacity3DS) {
        gfx_path.reserve(kMk64ExecStackCapacity3DS);
    }
    if (disp_stack.capacity() < 16) {
        disp_stack.reserve(16);
    }
    if (dlist != nullptr) {
        sMk64ExecStack3DS[sMk64ExecStackDepth3DS++] = dlist;
    }
}

void GfxExecStack::stop() {
    sMk64ExecStackDepth3DS = 0;
    gfx_path.clear();
    disp_stack.clear();
}

F3DGfx*& GfxExecStack::currCmd() {
    return Mk64ExecStackEmpty3DS()
               ? sMk64NullCommand3DS
               : sMk64ExecStack3DS[sMk64ExecStackDepth3DS - 1];
}

void GfxExecStack::openDisp(const char* file, int line) {
    disp_stack.push_back({ file, line });
}
void GfxExecStack::closeDisp() {
    if (!disp_stack.empty()) disp_stack.pop_back();
}
const std::vector<GfxExecStack::CodeDisp>& GfxExecStack::getDisp() const {
    return disp_stack;
}

void GfxExecStack::branch(F3DGfx* caller) {
    if (Mk64ExecStackEmpty3DS() ||
        sMk64ExecStackDepth3DS >= kMk64ExecStackCapacity3DS) {
        stop();
        return;
    }
    F3DGfx* old = sMk64ExecStack3DS[sMk64ExecStackDepth3DS - 1];
    sMk64ExecStack3DS[sMk64ExecStackDepth3DS - 1] = nullptr;
    sMk64ExecStack3DS[sMk64ExecStackDepth3DS++] = old;
    gfx_path.push_back(caller);
}

void GfxExecStack::call(F3DGfx* caller, F3DGfx* callee) {
    if (callee == nullptr || sMk64ExecStackDepth3DS >= kMk64ExecStackCapacity3DS) {
        stop();
        return;
    }
    sMk64ExecStack3DS[sMk64ExecStackDepth3DS++] = callee;
    gfx_path.push_back(caller);
}

F3DGfx* GfxExecStack::ret() {
    if (Mk64ExecStackEmpty3DS()) return nullptr;
    F3DGfx* cmd = sMk64ExecStack3DS[--sMk64ExecStackDepth3DS];
    if (!gfx_path.empty()) gfx_path.pop_back();
    while (!Mk64ExecStackEmpty3DS() && Mk64ExecStackCurrent3DS() == nullptr) {
        --sMk64ExecStackDepth3DS;
        if (!gfx_path.empty()) gfx_path.pop_back();
    }
    return cmd;
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_EXEC_STACK_ORIGINAL)
            message(FATAL_ERROR "The Fast3D execution-stack insertion point changed")
        endif()
        string(REPLACE
            "    while (!g_exec_stack.cmd_stack.empty()) {\n        auto cmd = g_exec_stack.cmd_stack.top();"
            "    size_t commandCount = 0;\n    constexpr size_t kMaxCommandsPerFrame = 250000;\n    constexpr size_t kMaxDisplayListDepth = 512;\n    while (!Mk64ExecStackEmpty3DS()) {\n        auto cmd = Mk64ExecStackCurrent3DS();\n        if (++commandCount > kMaxCommandsPerFrame || Mk64ExecStackDepth3DS() > kMaxDisplayListDepth) {\n            if (Mk64Diagnostics3DSGfxWatchdog != nullptr && cmd != nullptr) {\n                Mk64Diagnostics3DSGfxWatchdog(cmd, cmd->words.w0, cmd->words.w1, commandCount);\n            }\n            g_exec_stack.stop();\n            break;\n        }"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        set(SPAGHETTIKART_3DS_POINTER_GUARD_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[#else
    if (i <= 0x0FFFFFFF) {
        return false;
    }
#endif]=]
            [=[#else
#if defined(__3DS__)
    // 3DS application-heap pointers commonly live below 0x10000000, so the
    // desktop numeric-address guard rejects valid framebuffer and sprite data.
    // SegAddr rejects an unset or out-of-range segment with nullptr.
    if ((cmd->words.w1 & 1U) != 0 && i == 0) {
        gfx->GfxDpSetTextureImage(C0(21, 3), C0(19, 2), C0(0, 12) + 1,
                                  imgData, 0, rawTexMetdata, nullptr);
        return false;
    }
#else
    if (i <= 0x0FFFFFFF) {
        return false;
    }
#endif
#endif]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_POINTER_GUARD_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D texture pointer guard insertion point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_CACHE_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        # Animated eight-racer scenes continuously rotate through more than the
        # old 256-entry working set. The runtime selects 256 entries on Old 3DS
        # and up to 512 on New 3DS with a linear-memory reserve; the cap bounds every fixed
        # owner table without restoring desktop's 1024-entry memory footprint.
        string(REPLACE
            "#define TEXTURE_CACHE_MAX_SIZE 1024"
            "#define TEXTURE_CACHE_MAX_SIZE 512"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_TEXTURE_CACHE_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D texture cache limit insertion point changed")
        endif()
        string(PREPEND SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "#include \"texture_pressure_3ds.hpp\"\n")
        set(SPAGHETTIKART_3DS_TEXTURE_OWNER_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "void Interpreter::TextureCacheClear() {"
            [=[#if defined(__3DS__)
extern "C" size_t Mk64Graphics3DSTextureCacheCapacity(void) __attribute__((weak));
extern "C" size_t Mk64Graphics3DSTextureCacheSpillCapacity(void) __attribute__((weak));
extern "C" bool Mk64Graphics3DSFenceTextureReuseBatch(void) __attribute__((weak));

struct Mk64TextureOwners3DS {
    std::shared_ptr<Ship::IResource> image;
    std::array<std::shared_ptr<Ship::IResource>, 2> palettes;
    const uint8_t* imageBase = nullptr;
    std::array<const uint8_t*, 2> paletteBases = {};
};

static std::array<Mk64TextureOwners3DS, TEXTURE_CACHE_MAX_SIZE + 1>
    sMk64TextureOwners3DS;
static std::array<std::shared_ptr<Ship::IResource>, 2> sMk64PaletteOwners3DS;
static std::array<const uint8_t*, 2> sMk64PaletteOwnerBases3DS = {};
static std::array<uint32_t, TEXTURE_CACHE_MAX_SIZE + 1> sMk64TextureFrameUses3DS = {};
static uint32_t sMk64TextureFrameSerial3DS = 1;

struct Mk64BlendedTextureOwner3DS {
    const uint8_t* data = nullptr;
    std::shared_ptr<Fast::Texture> resource;
};
static std::array<Mk64BlendedTextureOwner3DS, TEXTURE_CACHE_MAX_SIZE>
    sMk64BlendedTextureOwners3DS;

static size_t Mk64TextureCacheCapacity3DS() {
    const size_t requested = Mk64Graphics3DSTextureCacheCapacity != nullptr
                                 ? Mk64Graphics3DSTextureCacheCapacity()
                                 : 256;
    return std::max<size_t>(1, std::min<size_t>(requested, TEXTURE_CACHE_MAX_SIZE));
}

static size_t Mk64TextureCacheSpillCapacity3DS() {
    const size_t requested = Mk64Graphics3DSTextureCacheSpillCapacity != nullptr
                                 ? Mk64Graphics3DSTextureCacheSpillCapacity()
                                 : 384;
    return std::max<size_t>(1, std::min<size_t>(requested, TEXTURE_CACHE_MAX_SIZE));
}

static void Mk64BeginTextureFrame3DS() {
    if (++sMk64TextureFrameSerial3DS == 0) {
        sMk64TextureFrameUses3DS.fill(0);
        sMk64TextureFrameSerial3DS = 1;
    }
}

static void Mk64MarkTextureUsed3DS(uint32_t textureId) {
    if (textureId < sMk64TextureFrameUses3DS.size()) {
        sMk64TextureFrameUses3DS[textureId] = sMk64TextureFrameSerial3DS;
    }
}

static bool Mk64TextureUsedThisFrame3DS(uint32_t textureId) {
    return textureId < sMk64TextureFrameUses3DS.size() &&
           sMk64TextureFrameUses3DS[textureId] == sMk64TextureFrameSerial3DS;
}

static bool Mk64AddressInRange3DS(const uint8_t* address, const uint8_t* base,
                                  size_t size) {
    if (address == nullptr || base == nullptr) return false;
    const uintptr_t value = reinterpret_cast<uintptr_t>(address);
    const uintptr_t start = reinterpret_cast<uintptr_t>(base);
    return value >= start && value - start < std::max<size_t>(size, 1);
}

static std::shared_ptr<Fast::Texture> Mk64FindBlendedTextureOwner3DS(
    const uint8_t* data) {
    for (const auto& entry : sMk64BlendedTextureOwners3DS) {
        if (entry.data == data) return entry.resource;
    }
    return {};
}

static bool Mk64RegisterBlendedTextureOwner3DS(
    const uint8_t* data, const std::shared_ptr<Fast::Texture>& resource) {
    if (data == nullptr || resource == nullptr) return true;
    for (auto& entry : sMk64BlendedTextureOwners3DS) {
        if (entry.data == data || entry.data == nullptr) {
            entry.data = data;
            entry.resource = resource;
            return true;
        }
    }
    return false;
}

static void Mk64UnregisterBlendedTextureOwner3DS(const uint8_t* data) {
    for (auto& entry : sMk64BlendedTextureOwners3DS) {
        if (entry.data == data) {
            entry = {};
            return;
        }
    }
}

static void Mk64RetainTextureOwners3DS(uint32_t textureId,
                                       const TextureCacheKey& key, const RDP* rdp) {
    if (textureId >= sMk64TextureOwners3DS.size() || rdp == nullptr) return;
    auto& owners = sMk64TextureOwners3DS[textureId];
    owners = {};

    if (auto blendedOwner = Mk64FindBlendedTextureOwner3DS(key.texture_addr)) {
        owners.image = blendedOwner;
        owners.imageBase = blendedOwner->ImageData;
    } else {
        for (const auto& loaded : rdp->loaded_texture) {
            const auto& imageOwner = loaded.raw_tex_metadata.resource;
            const uint8_t* resourceBase = imageOwner != nullptr
                                              ? imageOwner->ImageData
                                              : loaded.addr;
            const size_t resourceSize = imageOwner != nullptr
                                            ? imageOwner->ImageDataSize
                                            : std::max(loaded.orig_size_bytes,
                                                       loaded.size_bytes);
            if (Mk64AddressInRange3DS(key.texture_addr, loaded.addr,
                                      std::max(loaded.orig_size_bytes,
                                               loaded.size_bytes)) ||
                Mk64AddressInRange3DS(key.texture_addr, resourceBase,
                                      resourceSize)) {
                owners.image = imageOwner;
                owners.imageBase = resourceBase;
                break;
            }
        }
    }
    for (size_t palette = 0; palette < sMk64PaletteOwners3DS.size(); ++palette) {
        if (key.palette_addrs[palette] != nullptr &&
            key.palette_addrs[palette] == rdp->palette_dram_addr[palette]) {
            owners.palettes[palette] = sMk64PaletteOwners3DS[palette];
            owners.paletteBases[palette] = sMk64PaletteOwnerBases3DS[palette];
        }
    }
}

static void Mk64ReleaseTextureOwner3DS(uint32_t textureId) {
    if (textureId < sMk64TextureOwners3DS.size()) {
        sMk64TextureOwners3DS[textureId] = {};
        sMk64TextureFrameUses3DS[textureId] = 0;
    }
}

static bool Mk64TextureOwnerReferences3DS(uint32_t textureId,
                                          const uint8_t* address) {
    if (textureId >= sMk64TextureOwners3DS.size() || address == nullptr) return false;
    const auto& owners = sMk64TextureOwners3DS[textureId];
    if (owners.imageBase == address) return true;
    return owners.paletteBases[0] == address || owners.paletteBases[1] == address;
}

static void Mk64ResetTextureOwners3DS() {
    for (auto& owners : sMk64TextureOwners3DS) owners = {};
    for (auto& resource : sMk64PaletteOwners3DS) {
        resource.reset();
    }
    sMk64PaletteOwnerBases3DS.fill(nullptr);
    sMk64TextureFrameUses3DS.fill(0);
    for (auto& owner : sMk64BlendedTextureOwners3DS) owner = {};
}
#endif

void Interpreter::TextureCacheClear() {]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_TEXTURE_OWNER_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture-owner helper insertion point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_FREE_IDS_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "mTextureCache.map.reserve(TEXTURE_CACHE_MAX_SIZE);"
            "mTextureCache.map.reserve(TEXTURE_CACHE_MAX_SIZE);\n    mTextureCache.free_texture_ids.reserve(TEXTURE_CACHE_MAX_SIZE);"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_FREE_IDS_ORIGINAL)
            message(FATAL_ERROR "The Fast3D free-texture-ID reserve point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_OWNER_CLEAR_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    for (const auto& entry : mTextureCache.map) {
        mTextureCache.free_texture_ids.push_back(entry.second.texture_id);
    }]=]
            [=[    for (const auto& entry : mTextureCache.map) {
        mTextureCache.free_texture_ids.push_back(entry.second.texture_id);
#if defined(__3DS__)
        Mk64ReleaseTextureOwner3DS(entry.second.texture_id);
#endif
    }]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_TEXTURE_OWNER_CLEAR_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture-owner clear point changed")
        endif()
        string(REPLACE
            "    mTextureCache.map.clear();\n    mTextureCache.lru.clear();"
            "    mTextureCache.map.clear();\n    mTextureCache.lru.clear();\n#if defined(__3DS__)\n    mMaskedTextures.clear();\n    Mk64ResetTextureOwners3DS();\n#endif"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        set(SPAGHETTIKART_3DS_TEXTURE_CACHE_LOOKUP_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    if (mTextureCache.map.size() >= TEXTURE_CACHE_MAX_SIZE) {
        // Remove the texture that was least recently used
        it = mTextureCache.lru.front().it;
        mTextureCache.free_texture_ids.push_back(it->second.texture_id);
        for (int j = 0; j < SHADER_MAX_TEXTURES; j++) {
            if (mRenderingState.mTextures[j] == &*it)
                mRenderingState.mTextures[j] = nullptr;
        }
        mTextureCache.map.erase(it);
        mTextureCache.lru.pop_front();
    }

    uint32_t texture_id;]=]
            [=[    if (mTextureCache.map.size() >= Mk64TextureCacheCapacity3DS()) {
        // Never overwrite a backing texture that an earlier draw in this
        // submitted frame still references. Prefer a cold LRU node; if a frame
        // really needs more than the profile capacity, grow only up to the
        // compile-time bound and reclaim those spill nodes on later misses.
        auto lruLocation3DS = mTextureCache.lru.end();
        for (auto candidate3DS = mTextureCache.lru.begin();
             candidate3DS != mTextureCache.lru.end(); ++candidate3DS) {
            if (!Mk64TextureUsedThisFrame3DS(candidate3DS->it->second.texture_id)) {
                lruLocation3DS = candidate3DS;
                break;
            }
        }
        if (lruLocation3DS == mTextureCache.lru.end() &&
            mTextureCache.map.size() >= Mk64TextureCacheSpillCapacity3DS()) {
            // A single frame can exceed the resident cache. Submit its pending
            // vertices and fence the GPU without presenting or clearing it.
            // Only then may earlier batches' texture backing be overwritten.
            Flush();
            if (Mk64Graphics3DSFenceTextureReuseBatch == nullptr ||
                !Mk64Graphics3DSFenceTextureReuseBatch())
                throw mk64_3ds::TextureCachePressure();
            Mk64BeginTextureFrame3DS();
            // The current draw may have resolved tile 0 before a tile 1 miss.
            // Preserve every currently selected binding across the fence.
            for (auto* selected3DS : mRenderingState.mTextures) {
                if (selected3DS != nullptr)
                    Mk64MarkTextureUsed3DS(selected3DS->second.texture_id);
            }
            for (auto candidate3DS = mTextureCache.lru.begin();
                 candidate3DS != mTextureCache.lru.end(); ++candidate3DS) {
                if (!Mk64TextureUsedThisFrame3DS(candidate3DS->it->second.texture_id)) {
                    lruLocation3DS = candidate3DS;
                    break;
                }
            }
            if (lruLocation3DS == mTextureCache.lru.end())
                throw mk64_3ds::TextureCachePressure();
        }
        if (lruLocation3DS != mTextureCache.lru.end()) {
        it = lruLocation3DS->it;
        const uint32_t textureId3DS = it->second.texture_id;
        for (int j = 0; j < SHADER_MAX_TEXTURES; j++) {
            if (mRenderingState.mTextures[j] == &*it)
                mRenderingState.mTextures[j] = nullptr;
        }
        auto nodeHandle3DS = mTextureCache.map.extract(it);
        nodeHandle3DS.key() = key;
        auto insertResult3DS = mTextureCache.map.insert(std::move(nodeHandle3DS));
        assert(insertResult3DS.inserted);
        it = insertResult3DS.position;
        lruLocation3DS->it = it;
        mTextureCache.lru.splice(mTextureCache.lru.end(), mTextureCache.lru,
                                 lruLocation3DS);
        // Match the value-initialized state of an ordinary cache miss. Keeping
        // the evicted sampler metadata can make the interpreter skip a needed
        // sampler update for the new texture.
        it->second = TextureCacheValue{};
        it->second.texture_id = textureId3DS;
        it->second.lru_location = lruLocation3DS;

        // The cache no longer exposes the old key or node state. Release its
        // resource before retaining the new owner, preserving normal eviction
        // ordering while keeping the re-key operation exception-safe.
        Mk64ReleaseTextureOwner3DS(textureId3DS);
        Mk64RetainTextureOwners3DS(textureId3DS, key, mRdp);
        Mk64MarkTextureUsed3DS(textureId3DS);
        mRapi->SelectTexture(i, textureId3DS);
        mRapi->SetSamplerParameters(i, false, 0, 0);
        *n = &*it;
        return false;
        }
    }

    uint32_t texture_id;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_CACHE_LOOKUP_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture-cache reuse point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_FRAME_HIT_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    if (it != mTextureCache.map.end()) {
        mRapi->SelectTexture(i, it->second.texture_id);]=]
            [=[    if (it != mTextureCache.map.end()) {
        Mk64MarkTextureUsed3DS(it->second.texture_id);
        mRapi->SelectTexture(i, it->second.texture_id);]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_FRAME_HIT_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture frame-use hit point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_OWNER_RETAIN_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    node->second.lru_location = mTextureCache.lru.insert(mTextureCache.lru.end(), { it });

    mRapi->SelectTexture(i, texture_id);]=]
            [=[    node->second.lru_location = mTextureCache.lru.insert(mTextureCache.lru.end(), { it });
#if defined(__3DS__)
    Mk64RetainTextureOwners3DS(texture_id, key, mRdp);
    Mk64MarkTextureUsed3DS(texture_id);
#endif

    mRapi->SelectTexture(i, texture_id);]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_TEXTURE_OWNER_RETAIN_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture-owner retain point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_FRAME_BEGIN_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void Interpreter::Run(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtx_replacements) {
    SpReset();]=]
            [=[void Interpreter::Run(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtx_replacements) {
    Mk64BeginTextureFrame3DS();
    SpReset();]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_FRAME_BEGIN_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture frame-use begin point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_MISS_TRANSACTION_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    uint32_t texture_id;
    if (!mTextureCache.free_texture_ids.empty()) {
        texture_id = mTextureCache.free_texture_ids.back();
        mTextureCache.free_texture_ids.pop_back();
    } else {
        texture_id = mRapi->NewTexture();
    }

    it = mTextureCache.map.insert(std::make_pair(key, TextureCacheValue())).first;
    TextureCacheNode* node = &*it;
    node->second.texture_id = texture_id;
    node->second.lru_location = mTextureCache.lru.insert(mTextureCache.lru.end(), { it });
#if defined(__3DS__)
    Mk64RetainTextureOwners3DS(texture_id, key, mRdp);
    Mk64MarkTextureUsed3DS(texture_id);
#endif]=]
            [=[    // Allocate both CPU cache nodes before consuming a renderer ID. If
    // ordinary-heap pressure rejects either node, no unreachable GPU slot is
    // left behind to accumulate across recovery attempts.
    it = mTextureCache.map.insert(std::make_pair(key, TextureCacheValue())).first;
    TextureCacheNode* node = &*it;
    auto lruLocation3DS = mTextureCache.lru.end();
    uint32_t texture_id = 0;
    try {
        lruLocation3DS = mTextureCache.lru.insert(mTextureCache.lru.end(), { it });
        if (!mTextureCache.free_texture_ids.empty()) {
            texture_id = mTextureCache.free_texture_ids.back();
            mTextureCache.free_texture_ids.pop_back();
        } else {
            texture_id = mRapi->NewTexture();
        }
    } catch (...) {
        if (lruLocation3DS != mTextureCache.lru.end()) {
            mTextureCache.lru.erase(lruLocation3DS);
        }
        mTextureCache.map.erase(it);
        throw;
    }
    node->second.texture_id = texture_id;
    node->second.lru_location = lruLocation3DS;
#if defined(__3DS__)
    Mk64RetainTextureOwners3DS(texture_id, key, mRdp);
    Mk64MarkTextureUsed3DS(texture_id);
#endif]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_MISS_TRANSACTION_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture miss transaction point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_OWNER_ADDRESS_DELETE_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "            if (it->first.texture_addr == origAddr) {"
            "            if (it->first.texture_addr == origAddr ||\n                it->first.palette_addrs[0] == origAddr ||\n                it->first.palette_addrs[1] == origAddr ||\n                Mk64TextureOwnerReferences3DS(it->second.texture_id, origAddr)) {"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_OWNER_ADDRESS_DELETE_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture-owner address delete point changed")
        endif()
        set(SPAGHETTIKART_3DS_PALETTE_OWNER_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    const uint8_t* src = mRdp->texture_to_load.addr;
    uint32_t entryCount = high_index + 1;]=]
            [=[    const uint8_t* src = mRdp->texture_to_load.addr;
    const std::shared_ptr<Fast::Texture> paletteOwner3DS =
        mRdp->texture_to_load.raw_tex_metadata.resource;
    const uint8_t* paletteOwnerBase3DS = paletteOwner3DS != nullptr
                                             ? paletteOwner3DS->ImageData
                                             : src;
    if (src == nullptr) {
        mRdp->palette_dram_addr[0] = nullptr;
        mRdp->palette_dram_addr[1] = nullptr;
        sMk64PaletteOwners3DS[0].reset();
        sMk64PaletteOwners3DS[1].reset();
        sMk64PaletteOwnerBases3DS[0] = nullptr;
        sMk64PaletteOwnerBases3DS[1] = nullptr;
        return;
    }
    uint32_t entryCount = high_index + 1;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[            mRdp->palette_dram_addr[0] = src;
            mRdp->palette_dram_addr[1] = src + 256;]=]
            [=[            mRdp->palette_dram_addr[0] = src;
            mRdp->palette_dram_addr[1] = src + 256;
            sMk64PaletteOwners3DS[0] = paletteOwner3DS;
            sMk64PaletteOwners3DS[1] = paletteOwner3DS;
            sMk64PaletteOwnerBases3DS[0] = paletteOwnerBase3DS;
            sMk64PaletteOwnerBases3DS[1] = paletteOwnerBase3DS;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            "            mRdp->palette_dram_addr[0] = src;\n        } else {"
            "            mRdp->palette_dram_addr[0] = src;\n            sMk64PaletteOwners3DS[0] = paletteOwner3DS;\n            sMk64PaletteOwnerBases3DS[0] = paletteOwnerBase3DS;\n        } else {"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            "            mRdp->palette_dram_addr[1] = src;\n        }\n    } else {"
            "            mRdp->palette_dram_addr[1] = src;\n            sMk64PaletteOwners3DS[1] = paletteOwner3DS;\n            sMk64PaletteOwnerBases3DS[1] = paletteOwnerBase3DS;\n        }\n    } else {"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[        mRdp->palettes[1] = src;
        mRdp->palette_dram_addr[1] = src;]=]
            [=[        mRdp->palettes[1] = src;
        mRdp->palette_dram_addr[1] = src;
        sMk64PaletteOwners3DS[1] = paletteOwner3DS;
        sMk64PaletteOwnerBases3DS[1] = paletteOwnerBase3DS;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_PALETTE_OWNER_ORIGINAL)
            message(FATAL_ERROR "The Fast3D palette-owner insertion points changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_OWNER_DELETE_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[                mTextureCache.map.erase(it->first);
                again = true;]=]
            [=[                const uint32_t textureId3DS = it->second.texture_id;
                mTextureCache.map.erase(it->first);
#if defined(__3DS__)
                Mk64ReleaseTextureOwner3DS(textureId3DS);
#endif
                again = true;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_TEXTURE_OWNER_DELETE_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture-owner delete point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_DELETE_SCAN_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void Interpreter::TextureCacheDelete(const uint8_t* origAddr) {
    while (mTextureCache.map.bucket_count() > 0) {
        TextureCacheKey key = { origAddr, { 0 }, 0, 0, 0 }; // bucket index only depends on the address
        size_t bucket = mTextureCache.map.bucket(key);
        bool again = false;
        for (auto it = mTextureCache.map.begin(bucket); it != mTextureCache.map.end(bucket); ++it) {]=]
            [=[void Interpreter::TextureCacheDelete(const uint8_t* origAddr) {
    while (!mTextureCache.map.empty()) {
        bool again = false;
        // Palette addresses are not part of the unordered-map hash. Explicit
        // source eviction is transition-time work, so scan the bounded cache.
        for (auto it = mTextureCache.map.begin(); it != mTextureCache.map.end(); ++it) {]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_DELETE_SCAN_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture source-eviction scan changed")
        endif()
        set(SPAGHETTIKART_3DS_MATRIX_LOOKUP_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void Interpreter::GfxSpMatrix(uint8_t parameters, const int32_t* addr) {
    float matrix[4][4];

    if (auto it = mCurMtxReplacements->find((Mtx*)addr); it != mCurMtxReplacements->end()) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                float v = it->second.mf[i][j];]=]
            [=[void Interpreter::GfxSpMatrix(uint8_t parameters, const int32_t* addr) {
    float matrix[4][4];

    MtxF replacement3DS = {};
    const bool hasReplacement3DS = Mk64FrameInterpolation3DSLookup != nullptr &&
                                   Mk64FrameInterpolation3DSLookup((Mtx*)addr, &replacement3DS);
    auto replacementIt = mCurMtxReplacements->find((Mtx*)addr);
    if (hasReplacement3DS || replacementIt != mCurMtxReplacements->end()) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                float v = hasReplacement3DS ? replacement3DS.mf[i][j]
                                            : replacementIt->second.mf[i][j];]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_MATRIX_LOOKUP_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D matrix lookup insertion point changed")
        endif()
        set(SPAGHETTIKART_3DS_INTERPRETER_INSTANCE_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[static std::weak_ptr<Interpreter> mInstance;
// Set a cached pointer to the instance so we don't need to go through the window every time
void GfxSetInstance(std::shared_ptr<Interpreter> gfx) {
    mInstance = gfx;
}]=]
            [=[// 3DS command dispatch is synchronous and the runtime explicitly clears this
// pointer before releasing its owning shared_ptr. Avoid taking an atomic weak
// reference for every Fast3D command in large race display lists.
static void Mk64ResetTextureOwners3DS();
static Interpreter* mInstance = nullptr;
static float sMk64AspectCorrection3DS = 1.0f;
void GfxSetInstance(std::shared_ptr<Interpreter> gfx) {
    if (gfx == nullptr) {
        Mk64ResetTextureOwners3DS();
    }
    mInstance = gfx.get();
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            "mInstance.lock().get()"
            "mInstance"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_INTERPRETER_INSTANCE_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D interpreter instance insertion points changed")
        endif()
        set(SPAGHETTIKART_3DS_ASPECT_FACTOR_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    mCurDimensions.aspect_ratio = (float)mCurDimensions.width / (float)mCurDimensions.height;]=]
            [=[    mCurDimensions.aspect_ratio = (float)mCurDimensions.width / (float)mCurDimensions.height;
    sMk64AspectCorrection3DS = ::mk64_3ds::Fast3DAspectCorrection(
        static_cast<float>(mCurDimensions.width),
        static_cast<float>(mCurDimensions.height));]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[        return x * (4.0f / 3.0f) / ((float)mCurDimensions.width / (float)mCurDimensions.height);]=]
            [=[        return x * sMk64AspectCorrection3DS;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_ASPECT_FACTOR_ORIGINAL)
            message(FATAL_ERROR "The Fast3D aspect-factor insertion points changed")
        endif()
        set(SPAGHETTIKART_3DS_CURRENT_DIR_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "std::stack<std::string> currentDir;"
            ""
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            "    currentDir = std::stack<std::string>();\n"
            ""
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[void gfx_push_current_dir(char* path) {
    if (gfx_check_image_signature(path) == 1)
        path = &path[7];

    currentDir.push(GetPathWithoutFileName(path));
}]=]
            [=[void gfx_push_current_dir(char* path) {
    // No renderer path consumes this directory on 3DS. Keep the ABI entry
    // point while avoiding per-command string and deque allocations.
    (void)path;
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_CURRENT_DIR_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D current-directory insertion points changed")
        endif()
        set(SPAGHETTIKART_3DS_DEPTH_READ_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void Interpreter::GetPixelDepthPrepare(float x, float y) {
    AdjustPixelDepthCoordinates(x, y);
    mGetPixelDepthPending.emplace(x, y);
}

uint16_t Interpreter::GetPixelDepth(float x, float y) {
    AdjustPixelDepthCoordinates(x, y);

    if (auto it = mGetPixelDepthCached.find(std::make_pair(x, y)); it != mGetPixelDepthCached.end()) {
        return it->second;
    }

    mGetPixelDepthPending.emplace(x, y);

    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff> res =
        mRapi->GetPixelDepth(mRendersToFb ? mGameFb : 0, mGetPixelDepthPending);
    mGetPixelDepthCached.merge(res);
    mGetPixelDepthPending.clear();

    return mGetPixelDepthCached.find(std::make_pair(x, y))->second;
}]=]
            [=[void Interpreter::GetPixelDepthPrepare(float x, float y) {
    (void)x;
    (void)y;
}

uint16_t Interpreter::GetPixelDepth(float x, float y) {
    (void)x;
    (void)y;
    return 0;
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_DEPTH_READ_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D pixel depth insertion point changed")
        endif()
        set(SPAGHETTIKART_3DS_RESOURCE_NULL_GUARDS_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess((char*)data));
        texFlags = tex->Flags;]=]
            [=[            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess((char*)data));
        if (tex == nullptr || tex->ImageData == nullptr) {
            return;
        }
        texFlags = tex->Flags;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[    if (ucode_handler_index == ucode_f3dex2) {
        gfx->GfxSpMovememF3dex2(index, offset,
                                Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash));
    } else {
        auto light =
            (Fast::LightEntry*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);
        uintptr_t data = (uintptr_t)&light->Ambient;
        gfx->GfxSpMovememF3d(index, offset, (void*)(data + (hasOffset == 1 ? 0x8 : 0)));
    }]=]
            [=[    if (ucode_handler_index == ucode_f3dex2) {
        const void* data =
            Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);
        if (data != nullptr) {
            gfx->GfxSpMovememF3dex2(index, offset, data);
        }
    } else {
        auto* light =
            (Fast::LightEntry*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);
        if (light != nullptr) {
            const uintptr_t data = reinterpret_cast<uintptr_t>(&light->Ambient);
            gfx->GfxSpMovememF3d(index, offset,
                                 reinterpret_cast<void*>(data + (hasOffset == 1 ? 0x8 : 0)));
        }
    }]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[    F3DVtx* vtx =
        (F3DVtx*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer((const char*)fileName);
    vtx += vtxDataOff;

    gfx->GfxSpVertex(vtxCnt, vtxIdxOff, vtx);]=]
            [=[    F3DVtx* vtx =
        (F3DVtx*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer((const char*)fileName);
    if (vtx != nullptr) {
        gfx->GfxSpVertex(vtxCnt, vtxIdxOff, vtx + vtxDataOff);
    }]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[        } else {
            assert(0 && "???");
            // gfx_path.pop_back();
            // cmd = cmd_stack.top();
            // cmd_stack.pop();
        }]=]
            [=[        }]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_RESOURCE_NULL_GUARDS_ORIGINAL)
            message(FATAL_ERROR "The Fast3D resource null-guard insertion points changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_FAILURE_GUARDS_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[            if (tex == nullptr) {
                (*cmd0)++;
                return false;
            }]=]
            [=[            if (tex == nullptr || tex->ImageData == nullptr) {
                gfx->GfxDpSetTextureImage(C0(21, 3), C0(19, 2), C0(0, 12) + 1,
                                          imgData, 0, rawTexMetdata, nullptr);
                return false;
            }]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[    if (texture != nullptr) {
        Interpreter* gfx = mInstance;]=]
            [=[    if (texture != nullptr && texture->ImageData != nullptr) {
        Interpreter* gfx = mInstance;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[    } else {
        SPDLOG_ERROR("G_SETTIMG_OTR_FILEPATH: Texture is null");
    }
    return false;
}]=]
            [=[    } else {
        Interpreter* gfx = mInstance;
        gfx->GfxDpSetTextureImage(C0(21, 3), C0(19, 2), C0(0, 12) + 1,
                                  fileName, 0, RawTexMetadata{}, nullptr);
        SPDLOG_ERROR("G_SETTIMG_OTR_FILEPATH: Texture is null");
    }
    return false;
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[        Fast::Texture* tex = std::static_pointer_cast<Fast::Texture>(
                                 Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(
                                     reinterpret_cast<char*>(replacement)))
                                 .get();

        replacement = tex->ImageData;]=]
            [=[        std::shared_ptr<Fast::Texture> tex = std::static_pointer_cast<Fast::Texture>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(
                reinterpret_cast<char*>(replacement)));
        if (tex == nullptr || tex->ImageData == nullptr) {
            return;
        }
        replacement = tex->ImageData;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_FAILURE_GUARDS_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture failure guards changed")
        endif()
        set(SPAGHETTIKART_3DS_BLENDED_TEXTURE_OWNER_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void Interpreter::RegisterBlendedTexture(const char* name, uint8_t* mask, uint8_t* replacement) {
    if (gfx_check_image_signature(name)) {
        name += 7;
    }

    if (gfx_check_image_signature(reinterpret_cast<char*>(replacement))) {
        std::shared_ptr<Fast::Texture> tex = std::static_pointer_cast<Fast::Texture>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(
                reinterpret_cast<char*>(replacement)));
        if (tex == nullptr || tex->ImageData == nullptr) {
            return;
        }
        replacement = tex->ImageData;
    }

    mMaskedTextures[name] = MaskedTextureEntry{ mask, replacement };
}

void Interpreter::UnregisterBlendedTexture(const char* name) {
    if (gfx_check_image_signature(name)) {
        name += 7;
    }

    mMaskedTextures.erase(name);
}]=]
            [=[void Interpreter::RegisterBlendedTexture(const char* name, uint8_t* mask, uint8_t* replacement) {
    if (name == nullptr) return;
    if (gfx_check_image_signature(name)) {
        name += 7;
    }

    std::shared_ptr<Fast::Texture> replacementOwner3DS;
    if (gfx_check_image_signature(reinterpret_cast<char*>(replacement))) {
        replacementOwner3DS = std::static_pointer_cast<Fast::Texture>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(
                reinterpret_cast<char*>(replacement)));
        if (replacementOwner3DS == nullptr || replacementOwner3DS->ImageData == nullptr) {
            return;
        }
        replacement = replacementOwner3DS->ImageData;
    }

    if (auto previous3DS = mMaskedTextures.find(name);
        previous3DS != mMaskedTextures.end()) {
        Mk64UnregisterBlendedTextureOwner3DS(previous3DS->second.replacementData);
    }
    mMaskedTextures[name] = MaskedTextureEntry{ mask, replacement };
    if (!Mk64RegisterBlendedTextureOwner3DS(replacement, replacementOwner3DS)) {
        mMaskedTextures.erase(name);
    }
}

void Interpreter::UnregisterBlendedTexture(const char* name) {
    if (name == nullptr) return;
    if (gfx_check_image_signature(name)) {
        name += 7;
    }

    if (auto entry3DS = mMaskedTextures.find(name);
        entry3DS != mMaskedTextures.end()) {
        Mk64UnregisterBlendedTextureOwner3DS(entry3DS->second.replacementData);
        mMaskedTextures.erase(entry3DS);
    }
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_BLENDED_TEXTURE_OWNER_ORIGINAL)
            message(FATAL_ERROR "The Fast3D blended-texture owner point changed")
        endif()
        set(SPAGHETTIKART_3DS_HASH_TEXTURE_HANDLER_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[bool gfx_set_timg_otr_hash_handler_custom(F3DGfx** cmd0) {
    uintptr_t addr = (*cmd0)->words.w1;
    (*cmd0)++;
    uint64_t hash = ((uint64_t)(*cmd0)->words.w0 << 32) + (uint64_t)(*cmd0)->words.w1;

    const char* fileName =
        Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->HashToCString(hash);
    uint32_t texFlags = 0;
    RawTexMetadata rawTexMetadata = {};

    if (fileName == nullptr) {
        (*cmd0)++;
        return false;
    }

    std::shared_ptr<Fast::Texture> texture = std::static_pointer_cast<Fast::Texture>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(
            Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->HashToCString(hash)));
    if (texture != nullptr) {
        texFlags = texture->Flags;
        rawTexMetadata.width = texture->Width;
        rawTexMetadata.height = texture->Height;
        rawTexMetadata.h_byte_scale = texture->HByteScale;
        rawTexMetadata.v_pixel_scale = texture->VPixelScale;
        rawTexMetadata.type = texture->Type;
        rawTexMetadata.resource = texture;

        // OTRTODO: We have disabled caching for now to fix a texture corruption issue with HD texture
        // support. In doing so, there is a potential performance hit since we are not caching lookups. We
        // need to do proper profiling to see whether or not it is worth it to keep the caching system.

        char* tex = reinterpret_cast<char*>(texture->ImageData);

        if (tex != nullptr) {
            (*cmd0)--;
            uintptr_t oldData = (*cmd0)->words.w1;
            // TODO: wtf??
            (*cmd0)->words.w1 = (uintptr_t)tex;

            // if (ourHash != (uint64_t)-1) {
            //     auto res = ResourceLoad(ourHash);
            // }

            (*cmd0)++;
        }

        (*cmd0)--;
        F3DGfx* cmd = (*cmd0);
        uint32_t fmt = C0(21, 3);
        uint32_t size = C0(19, 2);
        uint32_t width = C0(0, 12) + 1;

        if (tex != NULL) {
            Interpreter* gfx = mInstance;
            gfx->GfxDpSetTextureImage(fmt, size, width, fileName, texFlags, rawTexMetadata, tex);
        }
    } else {
        SPDLOG_ERROR("G_SETTIMG_OTR_HASH: Texture is null");
    }

    (*cmd0)++;
    return false;
}]=]
            [=[bool gfx_set_timg_otr_hash_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance;
    F3DGfx* cmd = *cmd0;
    (*cmd0)++;
    const uint64_t hash =
        (static_cast<uint64_t>((*cmd0)->words.w0) << 32) |
        static_cast<uint64_t>((*cmd0)->words.w1);
    const char* fileName =
        Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->HashToCString(hash);
    const uint32_t fmt = C0(21, 3);
    const uint32_t size = C0(19, 2);
    const uint32_t width = C0(0, 12) + 1;
    RawTexMetadata rawTexMetadata = {};

    if (fileName == nullptr) {
        gfx->GfxDpSetTextureImage(fmt, size, width, nullptr, 0,
                                  rawTexMetadata, nullptr);
        return false;
    }

    std::shared_ptr<Fast::Texture> texture = std::static_pointer_cast<Fast::Texture>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(fileName));
    if (texture == nullptr || texture->ImageData == nullptr) {
        gfx->GfxDpSetTextureImage(fmt, size, width, fileName, 0,
                                  rawTexMetadata, nullptr);
        SPDLOG_ERROR("G_SETTIMG_OTR_HASH: Texture is null");
        return false;
    }

    rawTexMetadata.width = texture->Width;
    rawTexMetadata.height = texture->Height;
    rawTexMetadata.h_byte_scale = texture->HByteScale;
    rawTexMetadata.v_pixel_scale = texture->VPixelScale;
    rawTexMetadata.type = texture->Type;
    rawTexMetadata.resource = texture;
    gfx->GfxDpSetTextureImage(fmt, size, width, fileName, texture->Flags,
                              rawTexMetadata, texture->ImageData);
    return false;
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_HASH_TEXTURE_HANDLER_ORIGINAL)
            message(FATAL_ERROR "The Fast3D hashed texture handler changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_VALIDATION_ORDER_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    if (TextureCacheLookup(i, key)) {
        return;
    }

    // Guard against zero-sized textures that would cause divide-by-zero
    // or GPU API errors in UploadTexture.
    if (mRdp->texture_tile[tile].line_size_bytes == 0 || mRdp->loaded_texture[tmemIdex].size_bytes == 0 ||
        origAddr == nullptr) {
        return;
    }]=]
            [=[    // Never publish a cache key until its source has passed the
    // cheap validation gates. Otherwise an invalid upload becomes a permanent
    // false hit and keeps sampling the previous texture bound to this unit.
    if (mRdp->texture_tile[tile].line_size_bytes == 0 ||
        mRdp->loaded_texture[tmemIdex].size_bytes == 0 || origAddr == nullptr) {
        return;
    }

    const bool directUpload3DS =
        (texFlags & (TEX_FLAG_LOAD_AS_IMG | TEX_FLAG_LOAD_AS_RAW)) != 0;
    const bool supportedFormat3DS = directUpload3DS ||
        (fmt == G_IM_FMT_RGBA && (siz == G_IM_SIZ_16b || siz == G_IM_SIZ_32b)) ||
        (fmt == G_IM_FMT_IA && (siz == G_IM_SIZ_4b || siz == G_IM_SIZ_8b ||
                                siz == G_IM_SIZ_16b)) ||
        (fmt == G_IM_FMT_CI && (siz == G_IM_SIZ_4b || siz == G_IM_SIZ_8b ||
                                siz == G_IM_SIZ_16b || siz == G_IM_SIZ_32b)) ||
        (fmt == G_IM_FMT_I && (siz == G_IM_SIZ_4b || siz == G_IM_SIZ_8b));
    if (!supportedFormat3DS) {
        return;
    }
    if (!directUpload3DS && fmt == G_IM_FMT_CI) {
        if ((siz == G_IM_SIZ_4b && mRdp->palettes[paletteIndex / 8] == nullptr) ||
            (siz == G_IM_SIZ_8b &&
             (mRdp->palettes[0] == nullptr || mRdp->palettes[1] == nullptr))) {
            return;
        }
    }

    if (TextureCacheLookup(i, key)) {
        return;
    }]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_VALIDATION_ORDER_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture validation order changed")
        endif()
        set(SPAGHETTIKART_3DS_INTERPRETER_BOUNDS_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void* Interpreter::SegAddr(uintptr_t w1) {
    // Segmented?
    if (w1 & 1) {
        uint32_t segNum = (uint32_t)(w1 >> 24);

        uint32_t offset = w1 & 0x00FFFFFE;

        if (mSegmentPointers[segNum] != 0) {
            return (void*)(mSegmentPointers[segNum] + offset);
        } else {
            return (void*)w1;
        }
    } else {
        return (void*)w1;
    }
}]=]
            [=[void* Interpreter::SegAddr(uintptr_t w1) {
    if ((w1 & 1U) == 0) {
        return reinterpret_cast<void*>(w1);
    }
    const uint32_t segNum = static_cast<uint32_t>(w1 >> 24U);
    if (segNum >= MAX_SEGMENT_POINTERS || mSegmentPointers[segNum] == 0) {
        return nullptr;
    }
    const uint32_t offset = static_cast<uint32_t>(w1) & 0x00FFFFFEU;
    return reinterpret_cast<void*>(mSegmentPointers[segNum] + offset);
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[void Interpreter::GfxSpVertex(size_t n_vertices, size_t dest_index, const F3DVtx* vertices) {
    for (size_t i = 0; i < n_vertices; i++, dest_index++) {]=]
            [=[void Interpreter::GfxSpVertex(size_t n_vertices, size_t dest_index, const F3DVtx* vertices) {
    if (vertices == nullptr || dest_index >= MAX_VERTICES ||
        n_vertices > MAX_VERTICES - dest_index) {
        return;
    }
    for (size_t i = 0; i < n_vertices; i++, dest_index++) {]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[void Interpreter::GfxSpModifyVertex(uint16_t vtx_idx, uint8_t where, uint32_t val) {
    SUPPORT_CHECK(where == G_MWO_POINT_ST);]=]
            [=[void Interpreter::GfxSpModifyVertex(uint16_t vtx_idx, uint8_t where, uint32_t val) {
    if (vtx_idx >= MAX_VERTICES) return;
    SUPPORT_CHECK(where == G_MWO_POINT_ST);]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[void Interpreter::GfxSpTri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {
    struct LoadedVertex* v1 = &mRsp->loaded_vertices[vtx1_idx];]=]
            [=[void Interpreter::GfxSpTri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {
    // GfxDrawRectangle deliberately uses the four scratch vertices allocated
    // after the regular Fast3D vertex range. E5's generic bounds guard rejected
    // every textured rectangle, leaving menus black while triangle geometry
    // such as the title flag still rendered.
    if (!::mk64_3ds::AreFast3DTriangleVerticesValid(
            vtx1_idx, vtx2_idx, vtx3_idx, is_rect, MAX_VERTICES)) {
        return;
    }
    struct LoadedVertex* v1 = &mRsp->loaded_vertices[vtx1_idx];]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[        float dx1 = v1->x / (v1->w) - v2->x / (v2->w);
        float dy1 = v1->y / (v1->w) - v2->y / (v2->w);
        float dx2 = v3->x / (v3->w) - v2->x / (v2->w);
        float dy2 = v3->y / (v3->w) - v2->y / (v2->w);
        float cross = dx1 * dy2 - dy1 * dx2;]=]
            [=[        float cross = ::mk64_3ds::Fast3DProjectedCross(
            v1->x, v1->y, v1->w, v2->x, v2->y, v2->w,
            v3->x, v3->y, v3->w);]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            "    if ((mRsp->geometry_mode & cull_both) != 0) {"
            "    if ((mRsp->geometry_mode & cull_both) != 0 &&\n        ::mk64_3ds::Fast3DCanProject(v1->w, v2->w, v3->w)) {"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[void Interpreter::GfxSpMatrix(uint8_t parameters, const int32_t* addr) {
    float matrix[4][4];]=]
            [=[void Interpreter::GfxSpMatrix(uint8_t parameters, const int32_t* addr) {
    if (addr == nullptr) return;
    float matrix[4][4];]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[    } else {
        (*cmd0) = subGFX;
        g_exec_stack.branch(cmd);
        return true; // shortcut cmd increment
    }
    return false;]=]
            [=[    } else if (subGFX != nullptr) {
        (*cmd0) = subGFX;
        g_exec_stack.branch(cmd);
        return true; // shortcut cmd increment
    }
    return false;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_INTERPRETER_BOUNDS_ORIGINAL)
            message(FATAL_ERROR "The Fast3D bounds-guard insertion points changed")
        endif()
        set(SPAGHETTIKART_3DS_DEBUGGER_BYPASS_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "    auto dbg = mGfxDebugger;\n"
            ""
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[        if (dbg->IsDebugging()) {
            g_exec_stack.gfx_path.push_back(cmd);
            if (dbg->HasBreakPoint(g_exec_stack.gfx_path)) {
                // On a breakpoint with the active framebuffer still set, we need to reset back to prevent
                // soft locking the renderer
                if (mFbActive) {
                    mFbActive = 0;
                    mRapi->StartDrawToFramebuffer(mRendersToFb ? mGameFb : 0, 1);
                }

                break;
            }
            g_exec_stack.gfx_path.pop_back();
        }
        gfx_step();]=]
            [=[        gfx_step();]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_DEBUGGER_BYPASS_ORIGINAL)
            message(FATAL_ERROR "The Fast3D debugger bypass insertion points changed")
        endif()
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_INTERPRETER_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D interpreter watchdog insertion points changed")
        endif()
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
        include(platform/3ds/triangle_math.cmake)
        include(platform/3ds/fast3d_reuse.cmake)
        include(platform/3ds/cmake/native_geometry_3ds.cmake)
        include(platform/3ds/cmake/triangle_reuse_3ds.cmake)
        # Apply after the geometry patches, whose exact anchors contain the
        # original key declaration. Never let that ordering drop the repair.
        string(REPLACE "    ColorCombinerKey key;"
            "    ColorCombinerKey key{}; // initialize EVERY comparison field"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        if(NOT SPAGHETTIKART_3DS_INTERPRETER_TEXT MATCHES "ColorCombinerKey key\\{\\};")
            message(FATAL_ERROR "The Fast3D combiner key construction point changed")
        endif()
        include(platform/3ds/cmake/texture_owner_scan_3ds.cmake)
        file(WRITE "${SPAGHETTIKART_3DS_INTERPRETER_SOURCE}" "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
