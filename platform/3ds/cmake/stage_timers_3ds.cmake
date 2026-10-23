# Per-stage interpreter timing for physical-console dumps. The Old 3DS spends
# most of a racing frame inside Fast3D, and Azahar does not model its memory
# latency, so the split must be measured on hardware. Each timer costs two
# svcGetSystemTick calls; GfxSpTri1 is only counted, and its time is the
# remainder of interpreter_us. Every replacement checks its exact anchor.
macro(mk64_stage_replace old_text new_text description)
    string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "${old_text}" mk64_stage_anchor)
    if(mk64_stage_anchor EQUAL -1)
        message(FATAL_ERROR "Stage timer integration changed: ${description}")
    endif()
    string(REPLACE "${old_text}" "${new_text}"
        SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
endmacro()

mk64_stage_replace(
    "#include \"native_geometry_3ds.hpp\""
    "#include \"native_geometry_3ds.hpp\"\n#include \"performance_trace_3ds.hpp\""
    "includes")

mk64_stage_replace(
    [=[void Interpreter::Flush() {
    if (mBufVboLen > 0) {]=]
    [=[void Interpreter::Flush() {
    if (mBufVboLen > 0) {
        ::mk64_3ds::PerformanceTimer stageTimer3DS(::mk64_3ds::PerformanceCurrent().flush_us);]=]
    "flush")

mk64_stage_replace(
    [=[void Interpreter::GfxSpVertex(size_t n_vertices, size_t dest_index, const F3DVtx* vertices) {]=]
    [=[void Interpreter::GfxSpVertex(size_t n_vertices, size_t dest_index, const F3DVtx* vertices) {
    ::mk64_3ds::PerformanceTimer stageTimer3DS(::mk64_3ds::PerformanceCurrent().sp_vertex_us);
    ::mk64_3ds::PerformanceCurrent().sp_vertices += static_cast<uint32_t>(n_vertices);]=]
    "vertex load")

mk64_stage_replace(
    [=[void Interpreter::GfxSpTri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {]=]
    [=[void Interpreter::GfxSpTri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {
    ++::mk64_3ds::PerformanceCurrent().sp_tri_calls;]=]
    "triangle count")

mk64_stage_replace(
    [=[void Interpreter::ImportTexture(int i, int tile, bool importReplacement) {]=]
    [=[void Interpreter::ImportTexture(int i, int tile, bool importReplacement) {
    ::mk64_3ds::PerformanceTimer stageTimer3DS(::mk64_3ds::PerformanceCurrent().tex_import_us);
    ++::mk64_3ds::PerformanceCurrent().tex_imports;]=]
    "texture import")

mk64_stage_replace(
    [=[void Interpreter::TextureCacheDelete(const uint8_t* origAddr) {]=]
    [=[void Interpreter::TextureCacheDelete(const uint8_t* origAddr) {
    ::mk64_3ds::PerformanceTimer stageTimer3DS(::mk64_3ds::PerformanceCurrent().tex_delete_us);]=]
    "texture invalidation")

mk64_stage_replace(
    [=[void Interpreter::GfxSpMatrix(uint8_t parameters, const int32_t* addr) {]=]
    [=[void Interpreter::GfxSpMatrix(uint8_t parameters, const int32_t* addr) {
    ::mk64_3ds::PerformanceTimer stageTimer3DS(::mk64_3ds::PerformanceCurrent().matrix_us);
    ++::mk64_3ds::PerformanceCurrent().matrix_calls;]=]
    "matrix")

# Resource-path lookups for display lists, vertices, matrices and lights.
mk64_stage_replace(
    "#define C0(pos, width) ((cmd->words.w0 >> (pos)) & ((1U << width) - 1))"
    [=[namespace {
template <typename Key> auto Mk64TimedRawPointer3DS(Key key) {
    ::mk64_3ds::PerformanceTimer stageTimer3DS(::mk64_3ds::PerformanceCurrent().lookup_us);
    ++::mk64_3ds::PerformanceCurrent().lookups;
    return Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(key);
}
} // namespace

#define C0(pos, width) ((cmd->words.w0 >> (pos)) & ((1U << width) - 1))]=]
    "lookup helper")
mk64_stage_replace(
    "(F3DVtx*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer("
    "(F3DVtx*)Mk64TimedRawPointer3DS("
    "vertex lookups")
mk64_stage_replace(
    "(F3DGfx*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer("
    "(F3DGfx*)Mk64TimedRawPointer3DS("
    "display-list lookups")
mk64_stage_replace(
    "(const int32_t*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer("
    "(const int32_t*)Mk64TimedRawPointer3DS("
    "matrix lookups")

# Texture image resolution includes the texture resource lookup and decode.
foreach(mk64_stage_timg_handler
        "bool gfx_set_timg_handler_rdp(F3DGfx** cmd0) {"
        "bool gfx_set_timg_otr_hash_handler_custom(F3DGfx** cmd0) {"
        "bool gfx_set_timg_otr_filepath_handler_custom(F3DGfx** cmd0) {")
    mk64_stage_replace(
        "${mk64_stage_timg_handler}"
        "${mk64_stage_timg_handler}\n    ::mk64_3ds::PerformanceTimer stageTimer3DS(::mk64_3ds::PerformanceCurrent().timg_us);"
        "texture image ${mk64_stage_timg_handler}")
endforeach()
