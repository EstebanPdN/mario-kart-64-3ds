#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mk64_3ds {
// Fixed storage, single game-thread writer. No allocations or SD writes in
// the measured path. Each 256-tick window remains below 80 KiB on Old 3DS too.
// A second window retains the start of the latest race after the rolling one wraps.
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
    X(vertex_pack_us) X(ui_clean_us) X(ui_clean_bytes)
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
static_assert(sizeof(PerformanceHistory<256>) + sizeof(PerformanceTick) < 80 * 1024);
PerformanceTick& PerformanceCurrent();
std::uint64_t PerformanceNow();
void PerformanceBegin();
void PerformanceEnd();
void PerformanceResume();
void PerformanceRaceState(bool racing, std::uint32_t raceState, std::uint32_t timerMs);
void PerformanceArchiveRead(std::uint64_t start, std::uint32_t calls, std::uint32_t bytes);
bool PerformanceWrite(const char* directory);

class PerformanceTimer {
  public:
    explicit PerformanceTimer(std::uint32_t& output) : output(output), start(PerformanceNow()) {}
    ~PerformanceTimer() { output += static_cast<std::uint32_t>(PerformanceNow() - start); }
  private:
    std::uint32_t& output;
    std::uint64_t start;
};
}
