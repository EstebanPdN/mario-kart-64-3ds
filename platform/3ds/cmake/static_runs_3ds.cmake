# Static geometry run capture and replay (include/static_runs_3ds.hpp).
# Immutable display lists reached through resource commands record each
# vertex/triangle run once; later frames evaluate only its material and let
# PICA transform, cull, clip and depth-convert the recorded mesh. Runs after
# the stage timers and native geometry/lighting patches. Every replacement
# must match its anchor exactly once.
macro(mk64_static_replace old_text new_text description)
    string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "${old_text}" mk64_static_first)
    string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "${old_text}" mk64_static_last REVERSE)
    if(mk64_static_first EQUAL -1 OR NOT mk64_static_first EQUAL mk64_static_last)
        message(FATAL_ERROR "Static run integration changed: ${description}")
    endif()
    string(REPLACE "${old_text}" "${new_text}"
        SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
endmacro()

mk64_static_replace(
    "#include \"performance_trace_3ds.hpp\""
    "#include \"performance_trace_3ds.hpp\"\n#include \"static_runs_3ds.hpp\"\n#include \"original_fog_3ds.hpp\"\n#include \"system_3ds.h\"\n#include <3ds.h>"
    "includes")

mk64_static_replace(
    [=[static ::mk64_3ds::TriangleRun sTriangleRun3DS;]=]
    [=[static ::mk64_3ds::TriangleRun sTriangleRun3DS;
extern "C" bool Mk64Settings3DSGetStaticMeshes(void) __attribute__((weak));
extern "C" bool Mk64Graphics3DSResolvedNewModel(void) __attribute__((weak));
extern "C" uint32_t gMk64RaceResourceEpoch3DS __attribute__((weak));
extern "C" uint32_t Mk64Resource3DSGeneration(void) __attribute__((weak));
// Interpreted triangles by why they were not drawn from a recorded run:
// dynamic list, lighting, fog, texture generation, eligible, rectangle.
extern "C" uint64_t gMk64InterpretedTriangles3DS[6] __attribute__((weak));
namespace {
// Freed blocks may still be read by the GPU for the frame in flight.
struct Mk64StaticGrave3DS {
    void* pointer;
    uint32_t frame;
};
std::vector<Mk64StaticGrave3DS> sMk64StaticGraves3DS;
uint32_t sMk64StaticFrameSerial3DS = 1;
void* Mk64StaticAllocate3DS(size_t bytes) { return linearAlloc(bytes); }
void Mk64StaticRelease3DS(void* pointer) {
    if (pointer != nullptr) sMk64StaticGraves3DS.push_back({pointer, sMk64StaticFrameSerial3DS});
}
bool Mk64StaticPublish3DS(const void* pointer, size_t bytes) {
    return Mk64System3DSCleanDataCache(pointer, bytes);
}
uint32_t Mk64StaticSourceGeneration3DS() {
    return Mk64Resource3DSGeneration != nullptr ? Mk64Resource3DSGeneration() : 0;
}
bool Mk64StaticImmutable3DS(const void* pointer, size_t bytes) {
    // The executable's read-only CODE mapping is process-owned and cannot be
    // modified by course scroll/mirror writers or the hash-handler self patch.
    // Writable archive data remains hash-validated; residency is not proof.
    const uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
    if (bytes == 0 || address > UINT32_MAX || bytes > UINT32_MAX - address) return false;
    MemInfo info{};
    PageInfo page{};
    if (R_FAILED(svcQueryMemory(&info, &page, static_cast<uint32_t>(address)))) return false;
    return info.state == MEMSTATE_CODE && info.perm == MEMPERM_READ &&
        address >= info.base_addr && address + bytes <= uint64_t(info.base_addr) + info.size;
}
} // namespace
// Both models share a bounded 1 MiB linear budget for recorded geometry.
static ::mk64_3ds::StaticRunCache<MAX_VERTICES + 4> sStaticRuns3DS(1024U * 1024U, 768);
static bool sStaticMaterialOnly3DS = false;
struct Mk64StaticMaterial3DS {
    bool ok = false;
    int varyingInput = 0;
    bool constantColor = false;
    float color[4] = {};
    float colorMask[4] = {};
    float uvAffine[2][4] = {};
    float vertex[64] = {};
    ::mk64_3ds::NativeGeometryCull cull = ::mk64_3ds::NativeGeometryCull::None;
};
static Mk64StaticMaterial3DS sStaticMaterial3DS;
static const ::mk64_3ds::NativeGeometryDraw kMk64NoNativeDraw3DS{};
static bool Mk64InResourceFrame3DS();
// Recordable vertices have no lighting or generated UVs. Fog is evaluated
// from the exact load-time camera into a bounded per-frame stream. Their load
// signature is the exact pair of 16-bit texture scales, without a lossy hash.
static uint32_t Mk64StaticLoadSignature3DS(const RSP* rsp) {
    return static_cast<uint32_t>(rsp->texture_scaling_factor.s) |
           (static_cast<uint32_t>(rsp->texture_scaling_factor.t) << 16);
}
static bool Mk64StaticRecordable3DS(const RSP* rsp) {
    return (rsp->geometry_mode & (G_LIGHTING | G_TEXTURE_GEN)) == 0;
}
static ::mk64_3ds::StaticRunTransform Mk64StaticTransform3DS(Interpreter* gfx) {
    ::mk64_3ds::StaticRunTransform result;
    std::memcpy(result.matrix, gfx->mRsp->MP_matrix, sizeof(result.matrix));
    result.aspect = gfx->AdjXForAspectRatio(1.0f);
    result.fog = (gfx->mRsp->geometry_mode & G_FOG) != 0;
    result.fogMul = gfx->mRsp->fog_mul;
    result.fogOffset = gfx->mRsp->fog_offset;
    return result;
}
// A replayed slot owns its exact unlit colour/UVs and load-time transform.
// Materialize from those snapshots, not from a potentially changed source
// buffer under the current RSP state. No resource lookup or full vertex
// handler is needed at a boundary back to interpreted geometry.
static void Mk64DisableStaticLight3DS(size_t slot);
static void Mk64MaterializeSlot3DS(Interpreter* gfx, size_t slot) {
    if (!sStaticRuns3DS.IsVirtual(slot)) return;
    const auto& vertex = sStaticRuns3DS.Snapshot(slot);
    const auto& transform = sStaticRuns3DS.Transform(slot);
    auto& loaded = gfx->mRsp->loaded_vertices[slot];
    ::mk64_3ds::gNativeGeometry3DS.BeginLoad(slot, 1, transform.matrix, transform.aspect);
    ::mk64_3ds::gNativeGeometry3DS.Capture(slot, vertex.position, loaded);
    loaded.u = vertex.texcoord0[0];
    loaded.v = vertex.texcoord0[1];
    loaded.color.r = vertex.color[0];
    loaded.color.g = vertex.color[1];
    loaded.color.b = vertex.color[2];
    loaded.color.a = vertex.color[3];
    if (transform.fog)
        loaded.color.a = ::mk64_3ds::OriginalFogVertex(vertex.position, transform.matrix,
                                                     transform.fogMul, transform.fogOffset);
    Mk64DisableStaticLight3DS(slot);
    sStaticRuns3DS.Materialized(slot);
}]=]
    "static run state")

mk64_static_replace(
    "static ::mk64_3ds::NativeLightingVertices<MAX_VERTICES + 4> sNativeLights3DS;"
    "static ::mk64_3ds::NativeLightingVertices<MAX_VERTICES + 4> sNativeLights3DS;\nstatic void Mk64DisableStaticLight3DS(size_t slot) { sNativeLights3DS.Disable(slot); }"
    "materialized lighting state")

# --- Vertex loads: provenance and camera-independent snapshots.
mk64_static_replace(
    [=[        n_vertices > MAX_VERTICES - dest_index) {
        return;
    }
    ::mk64_3ds::gNativeGeometry3DS.BeginLoad(]=]
    [=[        n_vertices > MAX_VERTICES - dest_index) {
        return;
    }
    const bool staticLoad3DS = sStaticRuns3DS.Enabled() && Mk64InResourceFrame3DS() &&
                               Mk64StaticRecordable3DS(mRsp);
    const uint32_t staticSignature3DS = Mk64StaticLoadSignature3DS(mRsp);
    const auto staticTransform3DS = Mk64StaticTransform3DS(this);
    if (staticLoad3DS) sStaticRuns3DS.NoteLoadRange(vertices, n_vertices, dest_index);
    ::mk64_3ds::gNativeGeometry3DS.BeginLoad(]=]
    "vertex load provenance")

mk64_static_replace(
    [=[            d->color.a = v->cn[3];
        }
    }
}

void Interpreter::GfxSpModifyVertex(]=]
    [=[            d->color.a = v->cn[3];
        }
        if (staticLoad3DS) {
            ::mk64_3ds::StaticRunVertex static3DS{};
            for (int axis3DS = 0; axis3DS < 3; ++axis3DS) static3DS.position[axis3DS] = v->ob[axis3DS];
            static3DS.position[3] = 1.0f;
            static3DS.texcoord0[0] = static3DS.texcoord1[0] = d->u;
            static3DS.texcoord0[1] = static3DS.texcoord1[1] = d->v;
            static3DS.color[0] = d->color.r;
            static3DS.color[1] = d->color.g;
            static3DS.color[2] = d->color.b;
            static3DS.color[3] = v->cn[3];
            sStaticRuns3DS.NoteLoad(dest_index, &vertices[i], staticSignature3DS, static3DS, staticTransform3DS);
        } else {
            sStaticRuns3DS.ForgetSlot(dest_index);
        }
    }
}

void Interpreter::GfxSpModifyVertex(]=]
    "vertex snapshot")

mk64_static_replace(
    [=[    LoadedVertex* v = &mRsp->loaded_vertices[vtx_idx];
    v->u = s;]=]
    [=[    Mk64MaterializeSlot3DS(this, vtx_idx);
    sStaticRuns3DS.ForgetSlot(vtx_idx);
    LoadedVertex* v = &mRsp->loaded_vertices[vtx_idx];
    v->u = s;]=]
    "modified vertex")

# --- Material-only triangle evaluation used by replay.
mk64_static_replace(
    [=[void Interpreter::GfxSpTri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {]=]
    [=[extern "C" void Mk64Diagnostics3DSCheckpoint(const char* stage) __attribute__((weak));
// Log each material rejection reason once per session (renderer diagnostics).
static void Mk64NoteStaticReject3DS(uint32_t reasons) {
    static uint32_t seen = 0;
    if ((seen & reasons) == reasons) return;
    seen |= reasons;
    if (Mk64Diagnostics3DSCheckpoint == nullptr) return;
    char text[64];
    std::snprintf(text, sizeof(text), "static-run-material-rejected-0x%03lx", static_cast<unsigned long>(reasons));
    Mk64Diagnostics3DSCheckpoint(text);
}

// Reproduce one interpreter-layout vertex with the run's constant combiner
// inputs and tile clamps, plus the raw-UV affine maps. Unsupported materials
// leave ok=false and the run is interpreted normally.
static void Mk64FillStaticMaterial3DS(Interpreter* gfx, ColorCombiner* comb, uint8_t numInputs, bool use_alpha,
                                      bool use_fog, bool use_grayscale, uint32_t tm, const uint32_t* tex_width,
                                      const uint32_t* tex_height, const uint32_t* tex_width2,
                                      const uint32_t* tex_height2, const uint32_t* effective_tile,
                                      const bool* usedTextures, bool cullsBoth) {
    auto& material = sStaticMaterial3DS;
    material.ok = false;
    RDP* rdp = gfx->mRdp;
    const auto* program = gfx->mRenderingState.mShaderProgram;
    size_t stages = 0;
    if (program != nullptr)
        for (int cycle = 0; cycle < (program->twoCycle ? 2 : 1); ++cycle)
            stages += std::max(program->tevOperationCount[cycle][0], program->tevOperationCount[cycle][1]);
    // Texture unit 1's spare coordinate carries original fog separately from
    // primary alpha. Dual-texture fog retains the ordinary interpreter path.
    const uint32_t rejected = (use_fog && (program == nullptr || program->sampledTextures[1] || stages + 2 > 6) ? 1U : 0U) | (use_grayscale ? 2U : 0U) | (cullsBoth ? 4U : 0U) |
        ((rdp->other_mode_l & G_ZS_PRIM) != 0 ? 16U : 0U);
    if (rejected != 0) {
        Mk64NoteStaticReject3DS(rejected);
        return;
    }
    float* out = material.vertex;
    size_t count = 0;
    out[count++] = 0.0f;
    out[count++] = 0.0f;
    out[count++] = 0.0f;
    out[count++] = 1.0f;
    const bool linearFilter = (rdp->other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT;
    for (int t = 0; t < 2; ++t) {
        material.uvAffine[t][0] = material.uvAffine[t][1] = 0.0f;
        material.uvAffine[t][2] = material.uvAffine[t][3] = 0.0f;
        if (!usedTextures[t]) continue;
        if (tex_width[t] == 0 || tex_height[t] == 0) {
            Mk64NoteStaticReject3DS(32U);
            return;
        }
        const auto& tile = rdp->texture_tile[effective_tile[t]];
        float scaleS = 1.0f / 32.0f, scaleT = 1.0f / 32.0f;
        if (tile.shifts != 0) scaleS *= tile.shifts <= 10 ? 1.0f / (1 << tile.shifts) : float(1 << (16 - tile.shifts));
        if (tile.shiftt != 0) scaleT *= tile.shiftt <= 10 ? 1.0f / (1 << tile.shiftt) : float(1 << (16 - tile.shiftt));
        float offsetS = -tile.uls / 4.0f, offsetT = -tile.ult / 4.0f;
        if (linearFilter) {
            offsetS += 0.5f;
            offsetT += 0.5f;
        }
        const float inverseWidth = 1.0f / tex_width[t];
        const float inverseHeight = 1.0f / tex_height[t];
        material.uvAffine[t][0] = scaleS * inverseWidth;
        material.uvAffine[t][1] = scaleT * inverseHeight;
        material.uvAffine[t][2] = offsetS * inverseWidth;
        material.uvAffine[t][3] = offsetT * inverseHeight;
        out[count++] = 0.0f;
        out[count++] = 0.0f;
        if (tm & (1 << 2 * t)) out[count++] = (tex_width2[t] - 0.5f) * inverseWidth;
        if (tm & (1 << (2 * t + 1))) out[count++] = (tex_height2[t] - 0.5f) * inverseHeight;
    }
    if (use_fog) {
        const bool blend = (rdp->other_mode_l >> 30) == G_BL_CLR_BL;
        const auto& color = blend ? rdp->blend_color : rdp->fog_color;
        out[count++] = color.r / 255.0f;
        out[count++] = color.g / 255.0f;
        out[count++] = color.b / 255.0f;
        out[count++] = rdp->fog_color.a / 255.0f;
    }
    int shade = -1;
    for (int j = 0; j < numInputs; ++j) {
        for (int k = 0; k < 1 + (use_alpha ? 1 : 0); ++k) {
            const RGBA* color = nullptr;
            RGBA tmp{};
            switch (comb->shader_input_mapping[k][j]) {
                case G_CCMUX_PRIMITIVE: color = &rdp->prim_color; break;
                case G_CCMUX_SHADE:
                    if (shade >= 0 && shade != j) {
                        Mk64NoteStaticReject3DS(256U);
                        return;
                    }
                    shade = j;
                    tmp.r = tmp.g = tmp.b = tmp.a = 255;
                    color = &tmp;
                    break;
                case G_CCMUX_ENVIRONMENT: color = &rdp->env_color; break;
                case G_CCMUX_PRIMITIVE_ALPHA: tmp.r = tmp.g = tmp.b = rdp->prim_color.a; color = &tmp; break;
                case G_CCMUX_ENV_ALPHA: tmp.r = tmp.g = tmp.b = rdp->env_color.a; color = &tmp; break;
                case G_CCMUX_PRIM_LOD_FRAC: tmp.r = tmp.g = tmp.b = rdp->prim_lod_fraction; color = &tmp; break;
                case G_CCMUX_LOD_FRACTION:
                    // With G_TL_LOD the interpreter derives it from the first vertex's w.
                    if ((rdp->other_mode_l & G_TL_LOD) != 0) {
                        Mk64NoteStaticReject3DS(8U);
                        return;
                    }
                    tmp.r = tmp.g = tmp.b = tmp.a = 255;
                    color = &tmp;
                    break;
                case G_CCMUX_KEY_CENTER: color = &rdp->key_center; break;
                case G_CCMUX_KEY_SCALE: color = &rdp->key_scale; break;
                case G_CCMUX_CONVERT_K4: tmp.r = tmp.g = tmp.b = rdp->convert_k[4]; color = &tmp; break;
                case G_CCMUX_CONVERT_K5: tmp.r = tmp.g = tmp.b = rdp->convert_k[5]; color = &tmp; break;
                case G_ACMUX_PRIM_LOD_FRAC: tmp.a = rdp->prim_lod_fraction; color = &tmp; break;
                default: color = &tmp; break;
            }
            if (count + 3 > sizeof(material.vertex) / sizeof(material.vertex[0])) {
                Mk64NoteStaticReject3DS(64U);
                return;
            }
            if (k == 0) {
                out[count++] = color->r / 255.0f;
                out[count++] = color->g / 255.0f;
                out[count++] = color->b / 255.0f;
            } else {
                out[count++] = color->a / 255.0f;
            }
        }
    }
    // The varying input may mix shade and constant channels. Preserve each
    // channel independently, including opaque alpha when alpha is unused.
    if (shade < 0) shade = 0;
    const bool shadeRgb = numInputs != 0 && comb->shader_input_mapping[0][shade] == G_CCMUX_SHADE;
    const bool shadeAlpha = numInputs != 0 && comb->shader_input_mapping[1][shade] == G_CCMUX_SHADE;
    const bool standardFog = use_fog && (rdp->other_mode_l >> 30) != G_BL_CLR_BL;
    float input[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    if (numInputs != 0) {
        const size_t stride = use_alpha ? 4 : 3;
        const float* selected = out + count - stride * numInputs + stride * shade;
        for (size_t c = 0; c < stride; ++c) input[c] = selected[c];
    }
    if (standardFog && shadeAlpha) input[3] = 1.0f;
    material.constantColor = !shadeRgb || !use_alpha || !shadeAlpha || standardFog;
    ::mk64_3ds::StaticRunColor(input, shadeRgb, shadeAlpha && !standardFog, use_alpha, material.colorMask, material.color);
    material.varyingInput = shade;
    material.ok = true;
}

void Interpreter::GfxSpTri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {]=]
    "material helper")

mk64_static_replace(
    [=[            vtx1_idx, vtx2_idx, vtx3_idx, is_rect, MAX_VERTICES)) {
        return;
    }
    struct LoadedVertex* v1 = &mRsp->loaded_vertices[vtx1_idx];]=]
    [=[            vtx1_idx, vtx2_idx, vtx3_idx, is_rect, MAX_VERTICES)) {
        return;
    }
    if (!sStaticMaterialOnly3DS) {
        if (&gMk64InterpretedTriangles3DS[0] != nullptr) {
            size_t kind3DS = 4;
            if (is_rect) kind3DS = 5;
            else if (!Mk64InResourceFrame3DS()) kind3DS = 0;
            else if (mRsp->geometry_mode & G_LIGHTING) kind3DS = 1;
            else if (mRsp->geometry_mode & G_FOG) kind3DS = 2;
            else if (mRsp->geometry_mode & G_TEXTURE_GEN) kind3DS = 3;
            ++gMk64InterpretedTriangles3DS[kind3DS];
        }
        if (sStaticRuns3DS.Capturing()) {
            if (is_rect) sStaticRuns3DS.AbortCapture();
            else sStaticRuns3DS.CaptureTriangle(vtx1_idx, vtx2_idx, vtx3_idx);
        }
        if (!is_rect) {
            Mk64MaterializeSlot3DS(this, vtx1_idx);
            Mk64MaterializeSlot3DS(this, vtx2_idx);
            Mk64MaterializeSlot3DS(this, vtx3_idx);
        }
    }
    struct LoadedVertex* v1 = &mRsp->loaded_vertices[vtx1_idx];]=]
    "triangle capture")

mk64_static_replace(
    "    if (::mk64_3ds::CullDistantTriangle("
    "    if (!sStaticMaterialOnly3DS && ::mk64_3ds::CullDistantTriangle("
    "material-only distance")

mk64_static_replace(
    [=[    const auto& nativeDraw3DS = ::mk64_3ds::gNativeGeometry3DS.PrepareTriangle(
        vtx1_idx, vtx2_idx, vtx3_idx, mRsp->loaded_vertices,
        !is_rect && nativeCullMode3DS == 0, nativeCull3DS);]=]
    [=[    const auto& nativeDraw3DS = sStaticMaterialOnly3DS ? kMk64NoNativeDraw3DS :
        ::mk64_3ds::gNativeGeometry3DS.PrepareTriangle(
        vtx1_idx, vtx2_idx, vtx3_idx, mRsp->loaded_vertices,
        !is_rect && nativeCullMode3DS == 0, nativeCull3DS);
    if (sStaticMaterialOnly3DS) sStaticMaterial3DS.cull = nativeCull3DS;]=]
    "material-only native state")

mk64_static_replace(
    "    if (v1->clip_rej & v2->clip_rej & v3->clip_rej) {"
    "    if (!sStaticMaterialOnly3DS && (v1->clip_rej & v2->clip_rej & v3->clip_rej)) {"
    "material-only clipping")

mk64_static_replace(
    "    if (!nativeDraw3DS.enabled && (mRsp->geometry_mode & cull_both) != 0 &&"
    "    if (!sStaticMaterialOnly3DS && !nativeDraw3DS.enabled && (mRsp->geometry_mode & cull_both) != 0 &&"
    "material-only culling")

mk64_static_replace(
    "    const auto* usedTextures = material3DS.usedTextures;\n"
    [=[    const auto* usedTextures = material3DS.usedTextures;
    if (sStaticMaterialOnly3DS) {
        Mk64FillStaticMaterial3DS(this, comb, numInputs, use_alpha, use_fog, use_grayscale, tm, tex_width,
                                  tex_height, tex_width2, tex_height2, effective_tile, usedTextures,
                                  (mRsp->geometry_mode & cull_both) == cull_both);
        return;
    }
]=]
    "material-only exit")

# --- Resource frame tagging on the execution stack.
mk64_static_replace(
    "std::array<F3DGfx*, kMk64ExecStackCapacity3DS> sMk64ExecStack3DS = {};"
    "std::array<F3DGfx*, kMk64ExecStackCapacity3DS> sMk64ExecStack3DS = {};\n// True while a frame was entered through a resource display-list command.\nstd::array<bool, kMk64ExecStackCapacity3DS> sMk64ResourceFrame3DS = {};"
    "resource frame storage")
mk64_static_replace(
    "        sMk64ExecStack3DS[sMk64ExecStackDepth3DS++] = dlist;"
    "        sMk64ExecStack3DS[sMk64ExecStackDepth3DS++] = dlist;\n        sMk64ResourceFrame3DS[sMk64ExecStackDepth3DS - 1] = false;"
    "stack start")
mk64_static_replace(
    "    sMk64ExecStack3DS[sMk64ExecStackDepth3DS++] = old;"
    "    sMk64ExecStack3DS[sMk64ExecStackDepth3DS++] = old;\n    sMk64ResourceFrame3DS[sMk64ExecStackDepth3DS - 1] = false;"
    "stack branch")
mk64_static_replace(
    "    sMk64ExecStack3DS[sMk64ExecStackDepth3DS++] = callee;"
    "    sMk64ExecStack3DS[sMk64ExecStackDepth3DS++] = callee;\n    sMk64ResourceFrame3DS[sMk64ExecStackDepth3DS - 1] = false;"
    "stack call")
mk64_static_replace(
    [=[F3DGfx* Mk64ExecStackCurrent3DS() {
    return Mk64ExecStackEmpty3DS() ? nullptr
                                   : sMk64ExecStack3DS[sMk64ExecStackDepth3DS - 1];
}
}]=]
    [=[F3DGfx* Mk64ExecStackCurrent3DS() {
    return Mk64ExecStackEmpty3DS() ? nullptr
                                   : sMk64ExecStack3DS[sMk64ExecStackDepth3DS - 1];
}
}

static void Mk64MarkResourceFrame3DS() {
    if (sMk64ExecStackDepth3DS != 0) sMk64ResourceFrame3DS[sMk64ExecStackDepth3DS - 1] = true;
}

static bool Mk64InResourceFrame3DS() {
    return sMk64ExecStackDepth3DS != 0 && sMk64ResourceFrame3DS[sMk64ExecStackDepth3DS - 1];
}]=]
    "resource frame helpers")
mk64_static_replace(
    [=[        g_exec_stack.call(*cmd0, nDL);
    } else {
        if (nDL != nullptr) {
            (*cmd0) = nDL;
            g_exec_stack.branch(cmd);
            return true; // shortcut cmd increment]=]
    [=[        g_exec_stack.call(*cmd0, nDL);
        Mk64MarkResourceFrame3DS();
    } else {
        if (nDL != nullptr) {
            (*cmd0) = nDL;
            g_exec_stack.branch(cmd);
            Mk64MarkResourceFrame3DS();
            return true; // shortcut cmd increment]=]
    "resource path display lists")
mk64_static_replace(
    [=[        if (gfx != 0) {
            g_exec_stack.call(cmd, gfx);
        }]=]
    [=[        if (gfx != 0) {
            g_exec_stack.call(cmd, gfx);
            Mk64MarkResourceFrame3DS();
        }]=]
    "resource hash display lists")
mk64_static_replace(
    [=[            (*cmd0) = gfx;
            g_exec_stack.branch(cmd);
            return true; // shortcut cmd increment]=]
    [=[            (*cmd0) = gfx;
            g_exec_stack.branch(cmd);
            Mk64MarkResourceFrame3DS();
            return true; // shortcut cmd increment]=]
    "resource BranchZ display lists")

# --- Interpreted readers of vertex slots reload replayed slots first.
mk64_static_replace(
    "            ::mk64_3ds::gNativeGeometry3DS.Materialize(index3DS, gfx->mRsp->loaded_vertices[index3DS]);"
    "            Mk64MaterializeSlot3DS(gfx, index3DS);\n            ::mk64_3ds::gNativeGeometry3DS.Materialize(index3DS, gfx->mRsp->loaded_vertices[index3DS]);"
    "CULLDL slots")
mk64_static_replace(
    "    if (gfx->mRsp->loaded_vertices[vbidx].z <= zval ||"
    "    Mk64MaterializeSlot3DS(gfx, vbidx);\n    if (gfx->mRsp->loaded_vertices[vbidx].z <= zval ||"
    "BranchZ slot")

# --- Run detection and replay in the command loop.
mk64_static_replace(
    "static constexpr auto sMk64TriangleDispatch3DS = Mk64BuildTriangleDispatch3DS();"
    [=[static constexpr auto sMk64TriangleDispatch3DS = Mk64BuildTriangleDispatch3DS();
static constexpr auto Mk64BuildStaticRunDispatch3DS() {
    std::array<std::array<bool, 256>, ucode_max + 1> result{};
    for (size_t u = 0; u < result.size(); ++u)
        for (size_t op = 0; op < 256; ++op) {
            auto fn = sMk64Dispatch3DS[u][op];
            result[u][op] = sMk64TriangleDispatch3DS[u][op] || fn == gfx_vtx_handler_f3dex2 ||
                fn == gfx_vtx_handler_f3dex || fn == gfx_vtx_handler_f3d ||
                fn == gfx_vtx_hash_handler_custom || fn == gfx_vtx_otr_filepath_handler_custom;
        }
    return result;
}
static constexpr auto sMk64StaticRunDispatch3DS = Mk64BuildStaticRunDispatch3DS();]=]
    "run command table")

mk64_static_replace(
    "static void gfx_step() {"
    [=[static const F3DGfx* sStaticRunNext3DS = nullptr;
static uint32_t sStaticRaceEpoch3DS = 0;

static void Mk64StaticBeginFrame3DS() {
    ++sMk64StaticFrameSerial3DS;
    // Blocks released two frames ago can no longer be read by the GPU.
    auto& graves = sMk64StaticGraves3DS;
    size_t kept = 0;
    for (const auto& grave : graves) {
        if (sMk64StaticFrameSerial3DS - grave.frame >= 3) linearFree(grave.pointer);
        else graves[kept++] = grave;
    }
    graves.resize(kept);
    static bool allocatorReady = false;
    if (!allocatorReady) {
        sStaticRuns3DS.SetAllocator({Mk64StaticAllocate3DS, Mk64StaticRelease3DS, Mk64StaticPublish3DS});
        sStaticRuns3DS.SetValidation({Mk64StaticSourceGeneration3DS, Mk64StaticImmutable3DS});
        allocatorReady = true;
    }
    sStaticRuns3DS.SetEnabled(Mk64Settings3DSGetStaticMeshes == nullptr || Mk64Settings3DSGetStaticMeshes());
    // Track transitions free and reload resources; never key runs on stale addresses.
    const uint32_t epoch = &gMk64RaceResourceEpoch3DS != nullptr ? gMk64RaceResourceEpoch3DS : 0;
    if (epoch != sStaticRaceEpoch3DS) {
        sStaticRaceEpoch3DS = epoch;
        sStaticRuns3DS.Clear();
    }
    sStaticRuns3DS.BeginFrame(sMk64StaticFrameSerial3DS);
    sStaticRunNext3DS = nullptr;
}

static bool Mk64ReplayStaticRun3DS(Interpreter* gfx, const ::mk64_3ds::StaticRunCache<MAX_VERTICES + 4>::Entry& entry) {
    sStaticMaterial3DS = {};
    {
        struct MaterialScope {
            MaterialScope() { sStaticMaterialOnly3DS = true; }
            ~MaterialScope() { sStaticMaterialOnly3DS = false; }
        } materialScope;
        gfx->GfxSpTri1(0, 1, 2, false);
    }
    if (!sStaticMaterial3DS.ok) return false;
    gfx->Flush();
    StaticRunDraw3DS draw;
    draw.vertices = entry.vertices;
    draw.indices = entry.indices;
    draw.indexCount = entry.indexCount;
    draw.vertexCount = entry.vertexCount;
    draw.fogTransform = Mk64StaticTransform3DS(gfx);
    draw.fogFromVertex = (gfx->mRdp->other_mode_l >> 30) != G_BL_CLR_BL;
    std::memcpy(draw.center, entry.center, sizeof(draw.center));
    draw.materialVertex = sStaticMaterial3DS.vertex;
    draw.varyingInput = sStaticMaterial3DS.varyingInput;
    draw.constantColor = sStaticMaterial3DS.constantColor;
    std::memcpy(draw.color, sStaticMaterial3DS.color, sizeof(draw.color));
    std::memcpy(draw.colorMask, sStaticMaterial3DS.colorMask, sizeof(draw.colorMask));
    std::memcpy(draw.uvAffine, sStaticMaterial3DS.uvAffine, sizeof(draw.uvAffine));
    draw.cull = sStaticMaterial3DS.cull;
    static ::mk64_3ds::StaticRunMatrix matrix;
    matrix.Rows(Mk64StaticTransform3DS(gfx), entry.center, draw.rows);
    static_cast<GfxRenderingAPICitro3D*>(gfx->mRapi)->DrawStaticRun(draw);
    return true;
}

// Returns true when the command at `cmd` was replaced by a recorded run.
static bool Mk64StaticRunStep3DS(F3DGfx*& cmd, int8_t opcode, size_t ucode) {
    auto& runs = sStaticRuns3DS;
    const bool runCommand =
        sMk64StaticRunDispatch3DS[ucode < ucode_max ? ucode : ucode_max][static_cast<uint8_t>(opcode)];
    if (runs.Capturing()) {
        if (runCommand && cmd == sStaticRunNext3DS) return false;
        if (runs.EndCapture(sStaticRunNext3DS)) ++::mk64_3ds::PerformanceCurrent().static_captures;
    }
    if (!runCommand || !runs.Enabled() || !Mk64InResourceFrame3DS()) return false;
    Interpreter* gfx = mInstance;
    if (!Mk64StaticRecordable3DS(gfx->mRsp)) return false;
    const uint32_t signature = Mk64StaticLoadSignature3DS(gfx->mRsp);
    const auto transform = Mk64StaticTransform3DS(gfx);
    if (const auto* entry = runs.Find(cmd, signature, transform)) {
        if (Mk64ReplayStaticRun3DS(gfx, *entry)) {
            runs.Replayed(*entry, transform);
            auto& perf = ::mk64_3ds::PerformanceCurrent();
            ++perf.static_replays;
            perf.static_triangles += entry->indexCount / 3;
            cmd = const_cast<F3DGfx*>(static_cast<const F3DGfx*>(entry->end));
            return true;
        }
        ++::mk64_3ds::PerformanceCurrent().static_fallbacks;
        return false;
    }
    if (!runs.Known(cmd)) {
        sStaticRunNext3DS = nullptr;
        runs.BeginCapture(cmd, signature, transform);
    }
    return false;
}

// Lifetime counts for the diagnostic dump; no file I/O in the render path.
extern "C" void Mk64StaticRuns3DSStats(uint64_t out[9]) {
    const auto& counters = sStaticRuns3DS.Counters();
    out[0] = counters.captures;
    out[1] = counters.replays;
    out[2] = counters.replayedTriangles;
    out[3] = counters.rejected;
    out[4] = counters.fallbacks;
    out[5] = counters.materializations;
    out[6] = counters.evictions;
    out[7] = sStaticRuns3DS.EntryCount();
    out[8] = sStaticRuns3DS.Bytes();
}

extern "C" void Mk64StaticRuns3DSWorkStats(uint64_t out[3]) {
    const auto& counters = sStaticRuns3DS.Counters();
    out[0] = counters.snapshotCopies;
    out[1] = counters.transformCopies;
    out[2] = counters.hashedBytes;
}

static void gfx_step() {]=]
    "run step")

mk64_static_replace(
    [=[                               [static_cast<uint8_t>(opcode)]);
]=]
    [=[                               [static_cast<uint8_t>(opcode)]);
    if (Mk64StaticRunStep3DS(cmd, opcode, runUcode3DS)) return;
]=]
    "step hook")

mk64_static_replace(
    [=[                        (uint32_t)ucode_handler_index);
    }

    ++cmd;
}]=]
    [=[                        (uint32_t)ucode_handler_index);
    }

    ++cmd;
    if (sStaticRuns3DS.Capturing()) sStaticRunNext3DS = cmd;
}]=]
    "run continuation")

mk64_static_replace(
    [=[    sTriangleRun3DS.Invalidate();
    Mk64BeginTextureFrame3DS();]=]
    [=[    sTriangleRun3DS.Invalidate();
    Mk64BeginTextureFrame3DS();
    Mk64StaticBeginFrame3DS();]=]
    "frame start")

mk64_static_replace(
    [=[        gfx_step();
    }

    Flush();
    mGfxFrameBuffer = 0;]=]
    [=[        gfx_step();
    }
    if (sStaticRuns3DS.Capturing() && sStaticRuns3DS.EndCapture(sStaticRunNext3DS))
        ++::mk64_3ds::PerformanceCurrent().static_captures;

    Flush();
    mGfxFrameBuffer = 0;]=]
    "frame end")

mk64_static_replace(
    "        ::mk64_3ds::gNativeGeometry3DS.ResetVertices();"
    "        ::mk64_3ds::gNativeGeometry3DS.ResetVertices();\n        sStaticRuns3DS.Clear();"
    "interpreter destruction")

# Factor the actual production material resolver out of GfxSpTri1. Static
# replay calls it directly: no fabricated indices, geometry/culling, native
# state transition or triangle counter. Ordinary triangles use the same body.
set(mk64_material_begin "    struct TriangleMaterial3DS {")
set(mk64_material_end "    const auto comb = material3DS.comb;")
string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "${mk64_material_begin}" mk64_material_first)
string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "${mk64_material_end}" mk64_material_last)
if(mk64_material_first LESS 0 OR mk64_material_last LESS mk64_material_first)
    message(FATAL_ERROR "Shared static material resolver bounds changed")
endif()
math(EXPR mk64_material_length "${mk64_material_last}-${mk64_material_first}")
string(SUBSTRING "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" ${mk64_material_first} ${mk64_material_length} mk64_material_body)
string(FIND "${mk64_material_body}" "    static TriangleMaterial3DS savedMaterial3DS;" mk64_material_struct_end)
string(SUBSTRING "${mk64_material_body}" 0 ${mk64_material_struct_end} mk64_material_struct)
string(REPLACE "    };" "        bool hit = false, imported = false, reusable = false;\n    };" mk64_material_struct "${mk64_material_struct}")
string(SUBSTRING "${mk64_material_body}" ${mk64_material_struct_end} -1 mk64_material_function)
string(REPLACE "    static ::mk64_3ds::TriangleVertexCache<MAX_VERTICES> vertices3DS;\n" "" mk64_material_function "${mk64_material_function}")
foreach(mk64_material_member mRsp mRdp mRapi mRenderingState mShaderStack)
    string(REPLACE "${mk64_material_member}" "gfx->${mk64_material_member}" mk64_material_function "${mk64_material_function}")
endforeach()
foreach(mk64_material_method Flush ImportTexture ImportTextureMask LookupOrCreateColorCombiner LookupOrCreateShaderProgram)
    # Longest identifier first is not required: '(' prevents matching a prefix.
    string(REPLACE "${mk64_material_method}(" "gfx->${mk64_material_method}(" mk64_material_function "${mk64_material_function}")
endforeach()
string(REPLACE "this" "gfx" mk64_material_function "${mk64_material_function}")
set(mk64_material_helper "${mk64_material_struct}\nstatic TriangleMaterial3DS Mk64ResolveTriangleMaterial3DS(Interpreter* gfx, bool is_rect) {\n${mk64_material_function}\n    material3DS.hit = materialHit3DS;\n    material3DS.imported = imported3DS;\n    material3DS.reusable = reusable3DS;\n    return material3DS;\n}\n\n")
mk64_static_replace("${mk64_material_body}"
    "    const auto material3DS = Mk64ResolveTriangleMaterial3DS(this, is_rect);\n    const bool materialHit3DS = material3DS.hit, imported3DS = material3DS.imported, reusable3DS = material3DS.reusable;\n    static ::mk64_3ds::TriangleVertexCache<MAX_VERTICES> vertices3DS;\n"
    "shared material call")
mk64_static_replace("void Interpreter::GfxSpTri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {"
    "${mk64_material_helper}void Interpreter::GfxSpTri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {"
    "shared material resolver")
mk64_static_replace(
    [=[    {
        struct MaterialScope {
            MaterialScope() { sStaticMaterialOnly3DS = true; }
            ~MaterialScope() { sStaticMaterialOnly3DS = false; }
        } materialScope;
        gfx->GfxSpTri1(0, 1, 2, false);
    }]=]
    [=[    const auto material = Mk64ResolveTriangleMaterial3DS(gfx, false);
    Mk64FillStaticMaterial3DS(gfx, material.comb, material.numInputs, material.use_alpha,
        material.use_fog, material.use_grayscale, material.tm, material.tex_width, material.tex_height,
        material.tex_width2, material.tex_height2, material.effective_tile, material.usedTextures,
        (gfx->mRsp->geometry_mode & get_attr(CULL_BOTH)) == get_attr(CULL_BOTH));
    const auto mode = gfx->mRsp->geometry_mode & get_attr(CULL_BOTH);
    auto cull = ::mk64_3ds::NativeGeometryCull::None;
    if (mode == get_attr(CULL_FRONT)) cull = ::mk64_3ds::NativeGeometryCull::Front;
    if (mode == get_attr(CULL_BACK)) cull = ::mk64_3ds::NativeGeometryCull::Back;
    if ((gfx->mRsp->extra_geometry_mode & G_EX_INVERT_CULLING) != 0) {
        if (cull == ::mk64_3ds::NativeGeometryCull::Front) cull = ::mk64_3ds::NativeGeometryCull::Back;
        else if (cull == ::mk64_3ds::NativeGeometryCull::Back) cull = ::mk64_3ds::NativeGeometryCull::Front;
    }
    sStaticMaterial3DS.cull = cull;]=]
    "direct static material resolution")

include(platform/3ds/cmake/position_cache_3ds.cmake)
