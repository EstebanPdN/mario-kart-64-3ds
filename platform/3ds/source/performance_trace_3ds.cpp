#include "performance_trace_3ds.hpp"

#include <3ds.h>
#include <algorithm>
#include <atomic>
#include <cstdio>

namespace {
mk64_3ds::PerformanceHistory<256> sHistory;
mk64_3ds::PerformanceHistory<256> sRaceStart;
bool sRaceStartSeen = false, sCaptureRaceStart = false;
mk64_3ds::PerformanceTick sCurrent;
std::uint64_t sPreviousStart = 0;
std::uint32_t sTick = 0, sEpoch = 0;
// Resource reads may originate from the audio worker. Only cumulative atomic
// counters are touched there; trace rows remain game-thread-owned.
std::atomic<std::uint32_t> sReads{0}, sBytes{0}, sReadUs{0}, sFlushes{0}, sFlushUs{0};
std::atomic<std::uint32_t> sIoCalls{0}, sIoBytes{0}, sIoUs{0};
std::uint32_t sBeginIoCalls, sBeginIoBytes, sBeginIoUs;
std::uint32_t sBeginReads, sBeginBytes, sBeginReadUs, sBeginFlushes, sBeginFlushUs;
}
namespace mk64_3ds {
std::uint64_t PerformanceNow() {
    const std::uint64_t ticks = svcGetSystemTick();
    // Splitting the conversion avoids overflow after long system uptimes.
    return ticks / SYSCLOCK_ARM11 * 1000000ULL +
           ticks % SYSCLOCK_ARM11 * 1000000ULL / SYSCLOCK_ARM11;
}
PerformanceTick& PerformanceCurrent() { return sCurrent; }
void PerformanceBegin() {
    sCurrent = {};
    sCurrent.start_us = PerformanceNow();
    sCurrent.tick = ++sTick;
    sCurrent.epoch = sEpoch;
    sCurrent.interval_us = sPreviousStart == 0 ? 0 :
        static_cast<std::uint32_t>(std::min<std::uint64_t>(sCurrent.start_us - sPreviousStart, UINT32_MAX));
    sPreviousStart = sCurrent.start_us;
    sBeginIoCalls = sIoCalls.load(std::memory_order_relaxed);
    sBeginIoBytes = sIoBytes.load(std::memory_order_relaxed);
    sBeginIoUs = sIoUs.load(std::memory_order_relaxed);
    sBeginReads = sReads.load(std::memory_order_relaxed);
    sBeginBytes = sBytes.load(std::memory_order_relaxed);
    sBeginReadUs = sReadUs.load(std::memory_order_relaxed);
    sBeginFlushes = sFlushes.load(std::memory_order_relaxed);
    sBeginFlushUs = sFlushUs.load(std::memory_order_relaxed);
}
void PerformanceEnd() {
    sCurrent.total_us = static_cast<std::uint32_t>(PerformanceNow() - sCurrent.start_us);
    sCurrent.archive_io_calls = sIoCalls.load(std::memory_order_relaxed) - sBeginIoCalls;
    sCurrent.archive_io_bytes = sIoBytes.load(std::memory_order_relaxed) - sBeginIoBytes;
    sCurrent.archive_io_us = sIoUs.load(std::memory_order_relaxed) - sBeginIoUs;
    sCurrent.resource_reads = sReads.load(std::memory_order_relaxed) - sBeginReads;
    sCurrent.resource_bytes = sBytes.load(std::memory_order_relaxed) - sBeginBytes;
    sCurrent.resource_read_us = sReadUs.load(std::memory_order_relaxed) - sBeginReadUs;
    sCurrent.log_flushes = sFlushes.load(std::memory_order_relaxed) - sBeginFlushes;
    sCurrent.log_flush_us = sFlushUs.load(std::memory_order_relaxed) - sBeginFlushUs;
    sHistory.Push(sCurrent);
    if (sCaptureRaceStart) {
        sRaceStart.Push(sCurrent);
        if (sRaceStart.Size() == 256) sCaptureRaceStart = false;
    }
}
void PerformanceArchiveRead(std::uint64_t start, std::uint32_t calls, std::uint32_t bytes) {
    // RAM hits are part of resource processing, not physical SD latency.
    if (calls == 0) return;
    sIoCalls.fetch_add(calls, std::memory_order_relaxed);
    sIoBytes.fetch_add(bytes, std::memory_order_relaxed);
    sIoUs.fetch_add(static_cast<std::uint32_t>(PerformanceNow()-start), std::memory_order_relaxed);
}
void PerformanceRaceState(bool racing, std::uint32_t raceState, std::uint32_t timerMs) {
    sCurrent.race_state = raceState;
    sCurrent.course_timer_ms = timerMs;
    if (!racing || raceState < 3) {
        sRaceStartSeen = false;
        sCaptureRaceStart = false;
    }
    if (racing && raceState == 3 && !sRaceStartSeen) {
        sRaceStartSeen = true;
        sCaptureRaceStart = true;
        sRaceStart.Clear();
        // Retain up to one second of countdown before the first driving tick.
        const size_t begin = sHistory.Size() > 32 ? sHistory.Size() - 32 : 0;
        for (size_t i = begin; i < sHistory.Size(); ++i) {
            const auto& row = sHistory.At(i);
            if (row.epoch == sEpoch && row.game_state == sCurrent.game_state) sRaceStart.Push(row);
        }
    }
}

static bool WriteCsv(const char* path, const PerformanceHistory<256>& history) {
    FILE* file = std::fopen(path, "wb");
    if (file == nullptr) return false;
    std::fputs("start_us", file);
#define MK64_PERF_HEADER(name) std::fputs("," #name, file);
    MK64_PERF_FIELDS(MK64_PERF_HEADER)
#undef MK64_PERF_HEADER
    std::fputc('\n', file);
    for (std::size_t i = 0; i < history.Size(); ++i) {
        const auto& row = history.At(i);
        std::fprintf(file, "%llu", static_cast<unsigned long long>(row.start_us));
#define MK64_PERF_VALUE(name) std::fprintf(file, ",%lu", static_cast<unsigned long>(row.name));
        MK64_PERF_FIELDS(MK64_PERF_VALUE)
#undef MK64_PERF_VALUE
        std::fputc('\n', file);
    }
    bool ok = std::ferror(file) == 0;
    if (std::fclose(file) != 0) ok = false;
    return ok;
}
void PerformanceResume() {
    ++sEpoch;
    sPreviousStart = 0;
}
bool PerformanceWrite(const char* directory) {
    char path[256];
    std::snprintf(path, sizeof(path), "%s/performance.csv", directory);
    bool ok = WriteCsv(path, sHistory);
    std::snprintf(path, sizeof(path), "%s/race-start.csv", directory);
    ok &= WriteCsv(path, sRaceStart);
    FILE* file = nullptr;
    std::snprintf(path, sizeof(path), "%s/performance.txt", directory);
    file = std::fopen(path, "wb");
    if (file == nullptr) return false;
    std::fprintf(file,
        "Performance trace v5; latest %lu completed simulation ticks; recorder %lu bytes.\n"
        "Times are microseconds from the system tick clock. No dump-writing time is in these rows.\n"
        "Epoch changes after a diagnostic pause; never compute FPS across epochs.\n"
        "presents counts submitted images, not game logic ticks or physical scanouts.\n"
        "iteration_us includes graphics_us; graphics_us includes begin_wait/interpreter/hud/submit/interpolation.\n"
        "begin_wait_us measures C3D_FrameBegin inside Run: VBlank and previous GPU completion; it overlaps interpreter_us.\n"
        "key_cpu_us/mid_cpu_us: Citro3D CPU recording time of that submission (excludes begin wait).\n"
        "previous_gpu_us: latest completed GPU submission observed after StartFrame; not current-frame latency.\n"
        "decision_cpu_us/decision_gpu_us and audio_decision are exactly the values used by the adaptive gate.\n"
        "resource_read_us includes archive read/decompression, overlaps iteration/graphics, and may include worker work.\n"
        "archive_io_calls/bytes count underlying fread operations/bytes; archive_io_us times the read-ahead callback, including seeks/copies but excluding decompression.\n"
        "log_flush_us is worker SD flush time completed within this tick; may overlap main-thread work.\n"
        "pressure bits: 1 no prior image, 2 audio low, 4 slow presentation, 8 resource burst,\n"
        "16 texture burst, 32 Citro3D busy, 64 recovery. Zero on skipped/non-adaptive ticks is not a health verdict.\n"
        "profile: 0 Old / 1 New; distance: 0 Low / 1 Normal / 2 High.\n"
        "race-start.csv pins the latest race's first driving ticks plus up to 32 countdown ticks; pauses create epochs there too.\n"
        "game_state/race_state are original game enums; course_timer_ms is the course clock before this tick.\n"
        "interp_result: 0 no attempt, 1 ready, 2 disabled, 3 empty, 4 overflow, 5 camera cut, 6 signature table, 7 sequence table, 8 prepared table, 9 insufficient matches.\n"
        "interp_current/previous are recording counts; matched/total count unique final destinations. Flags: 1 current overflow, 2 previous overflow, 4 camera cut.\n"
        "Adaptive fields are sampled before preparation. Invalid scene pairs skip one midpoint on an established path; structural failures trigger a 15-tick cooldown.\n"
        "catchup_key: mandatory keyframe kept while omitting a midpoint to recover the simulation deadline.\n"
        "sync_grace: an isolated measured display wait did not trigger slow-tick pressure.\n"
        "GPU/CPU/substage timings overlap; do not add all columns together.\n",
        static_cast<unsigned long>(sHistory.Size()), static_cast<unsigned long>(sizeof(sHistory) + sizeof(sRaceStart) + sizeof(sCurrent)));
    // Describe only the current uninterrupted epoch; paused history remains in CSV.
    std::uint64_t duration = 0, epochStart = 0, epochEnd = 0, images = 0, skipped = 0, midpoints = 0;
    std::uint32_t worst = 0, count = 0;
    const std::uint32_t epoch = sHistory.Size() == 0 ? 0 : sHistory.At(sHistory.Size()-1).epoch;
    for (std::size_t i = 0; i < sHistory.Size(); ++i) {
        const auto& row = sHistory.At(i);
        if (row.epoch != epoch) continue;
        if (count == 0) epochStart = row.start_us;
        epochEnd = row.start_us + row.total_us;
        ++count; images += row.presents;
        skipped += row.suppressed; midpoints += row.presents == 2;
        worst = std::max(worst, row.total_us);
    }
    duration = epochEnd - epochStart;
    std::fprintf(file, "\nCurrent epoch %lu: ticks=%lu elapsed_us=%llu submitted=%llu midpoint_ticks=%llu suppressed=%llu worst_tick_us=%lu\n",
        static_cast<unsigned long>(epoch), static_cast<unsigned long>(count),
        static_cast<unsigned long long>(duration), static_cast<unsigned long long>(images),
        static_cast<unsigned long long>(midpoints), static_cast<unsigned long long>(skipped), static_cast<unsigned long>(worst));
    if (duration != 0) std::fprintf(file, "Active-window submission FPS=%.2f; simulation ticks/s=%.2f\n",
        images * 1000000.0 / duration, count * 1000000.0 / duration);
    ok &= std::ferror(file) == 0;
    if (std::fclose(file) != 0) ok = false;
    return ok;
}
}
extern "C" std::uint64_t Mk64Perf3DSResourceStart() { return mk64_3ds::PerformanceNow(); }
extern "C" void Mk64Perf3DSResourceRead(std::uint64_t start, std::size_t bytes) {
    sReads.fetch_add(1, std::memory_order_relaxed);
    sBytes.fetch_add(static_cast<std::uint32_t>(bytes), std::memory_order_relaxed);
    sReadUs.fetch_add(static_cast<std::uint32_t>(mk64_3ds::PerformanceNow()-start), std::memory_order_relaxed);
}
extern "C" void Mk64Perf3DSLogFlush(std::uint64_t start) {
    sFlushes.fetch_add(1, std::memory_order_relaxed);
    sFlushUs.fetch_add(static_cast<std::uint32_t>(mk64_3ds::PerformanceNow()-start), std::memory_order_relaxed);
}
