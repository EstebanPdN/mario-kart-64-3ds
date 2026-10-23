#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "performance_trace_3ds.h"

namespace mk64_3ds {
#define MK64_PERF_ART_FIELDS(X) X(art_requests) X(art_cache_hits) X(art_io_us) X(art_io_bytes) X(art_decode_us)
// Single game-thread writer. Storage is reserved once before archive admission;
// no allocations or SD writes in measured ticks. A second window pins race start.
#define MK64_PERF_DETAIL_FIELDS(X) \
    X(vertex_pack_us) X(sp_vertex_us) X(sp_vertices) X(sp_tri_calls) X(flush_us) \
    X(tex_import_us) X(tex_imports) X(tex_delete_us) X(matrix_us) X(matrix_calls) \
    X(lookup_us) X(lookups) X(timg_us)
#define MK64_PERF_FIELDS(X) \
    X(tick) X(epoch) X(interval_us) X(total_us) X(prepare_us) X(iteration_us) \
    X(graphics_us) X(audio_pump_us) X(sleep_us) X(lateness_us) \
    X(begin_wait_us) X(interpreter_us) X(hud_us) X(submit_us) X(interpolation_us) \
    X(key_cpu_us) X(mid_cpu_us) X(previous_gpu_us) X(decision_cpu_us) X(decision_gpu_us) \
    X(audio_decision) X(audio_after) X(pressure) X(healthy) X(probe) X(cooldown) \
    X(enabled) X(requested_mid) X(presents) X(suppressed) X(fault) X(pacer_reset) \
    X(draws) X(triangles) X(uploads) X(upload_bytes) X(culled) \
    X(resource_reads) X(resource_bytes) X(resource_read_us) X(archive_io_calls) X(archive_io_bytes) X(archive_io_us) \
    X(log_flushes) X(log_flush_us) X(scale) X(distance) X(layout) X(profile) X(width) X(filter) \
    X(interp_result) X(interp_current) X(interp_previous) X(interp_matched) X(interp_total) X(interp_flags) \
    X(game_state) X(race_state) X(course_timer_ms) \
    X(audio_synth_us) X(audio_wait_us) X(audio_blocks) X(catchup_key) X(sync_grace) \
    X(vertex_pack_us) X(ui_clean_us) X(ui_clean_bytes) \
    X(present_wait_us) X(native_io_us) X(native_io_bytes) X(native_decode_us) \
    X(native_cache_misses) X(native_cache_slots) X(native_resident_bytes) X(native_page_decodes) \
    X(native_raw_resident_bytes) X(native_raw_uploads) X(native_palette_hits) \
    X(sp_vertex_us) X(sp_vertices) X(sp_tri_calls) X(flush_us) X(tex_import_us) X(tex_imports) \
    X(tex_delete_us) X(matrix_us) X(matrix_calls) X(lookup_us) X(lookups) X(timg_us) \
    X(static_replays) X(static_triangles) X(static_captures) X(static_fallbacks) \
    MK64_PERF_ART_FIELDS(X) \
    X(detail_sampled) X(detail_period) X(resume_reason) X(excluded_us) \
    X(logic_barrier_us) X(events_us) X(controller_us) X(course_generation) X(course_id) \
    X(game_state_after) X(race_state_after) X(course_timer_after_ms) X(transition) \
    X(physics_us) X(collision_us) X(players_us) X(actors_us) X(camera_us) X(visual_us) X(leaf_faults)
struct PerformanceTick {
    std::uint64_t start_us = 0;
#define MK64_PERF_MEMBER(name) std::uint32_t name = 0;
    MK64_PERF_FIELDS(MK64_PERF_MEMBER)
#undef MK64_PERF_MEMBER
};

template <std::size_t Capacity> class PerformanceHistory {
  public:
    void Push(const PerformanceTick& tick) {
        rows[next] = tick;
        next = (next + 1) % Capacity;
        if (count < Capacity) ++count;
    }
    std::size_t Size() const { return count; }
    void Clear() { next = count = 0; }
    const PerformanceTick& At(std::size_t chronologicalIndex) const {
        return rows[(next + Capacity - count + chronologicalIndex) % Capacity];
    }
  private:
    std::array<PerformanceTick, Capacity> rows = {};
    std::size_t next = 0;
    std::size_t count = 0;
};
#ifndef MK64_3DS_AUTODUMP_LAB
static_assert(sizeof(PerformanceHistory<256>) + sizeof(PerformanceTick) < 128 * 1024);
#else
static_assert(sizeof(PerformanceHistory<1024>) < 544 * 1024);
#endif
// A planning floor, not measured peak headroom. Both pools and fallback choice
// are reported. Failure never silently shortens the diagnostic history.
constexpr std::size_t kPerformanceLinearReserve = 6 * 1024 * 1024;
bool PerformanceInitialize();
void PerformanceShutdown(); // Main-thread only, after producers have stopped.
const char* PerformanceStorageKind();
std::size_t PerformanceStorageBytes();
std::size_t PerformanceStorageReservedBytes(); // Linear alignment included; malloc overhead is separate.
bool PerformanceDetailsEnabled();
bool PerformanceTimerEnabled(const std::uint32_t* output);
enum class PerformancePause : std::uint32_t { None, Diagnostic, LabExport, Suspend, Terminal, ResourceLoad };
PerformanceTick& PerformanceCurrent();
std::uint64_t PerformanceNow();
void PerformanceBegin();
void PerformanceEnd();
void PerformanceResume(PerformancePause reason = PerformancePause::Diagnostic, std::uint64_t start = 0);
void PerformanceAbandon(); // Discard an incomplete envelope; never publish export work.
void PerformanceRaceState(bool racing, std::uint32_t raceState, std::uint32_t timerMs);
void PerformanceStateAfter(std::uint32_t gameState, std::uint32_t raceState, std::uint32_t timerMs);
void PerformanceCourse(std::uint32_t generation, std::uint32_t courseId);
void PerformanceArchiveRead(std::uint64_t start, std::uint32_t calls, std::uint32_t bytes);
bool PerformanceWrite(const char* directory);

class PerformanceTimer {
  public:
    explicit PerformanceTimer(std::uint32_t& output)
        : output(output), enabled(PerformanceTimerEnabled(&output)),
          start(enabled ? PerformanceNow() : 0) {}
    ~PerformanceTimer() { if (enabled) output += static_cast<std::uint32_t>(PerformanceNow() - start); }
    PerformanceTimer(const PerformanceTimer&) = delete;
    PerformanceTimer& operator=(const PerformanceTimer&) = delete;
  private:
    std::uint32_t& output;
    bool enabled;
    std::uint64_t start;
};
class PerformanceGameTimer {
  public:
    explicit PerformanceGameTimer(Mk64PerfGameSection3DS section) : token(Mk64Perf3DSGameSectionBegin(section)) {}
    ~PerformanceGameTimer() { Mk64Perf3DSGameSectionEnd(token); }
    PerformanceGameTimer(const PerformanceGameTimer&) = delete;
    PerformanceGameTimer& operator=(const PerformanceGameTimer&) = delete;
  private:
    std::uint32_t token;
};
}
