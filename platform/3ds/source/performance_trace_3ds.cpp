#include "performance_trace_3ds.hpp"

#include <3ds.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#ifndef MK64_3DS_PERF_DETAIL_PERIOD
#define MK64_3DS_PERF_DETAIL_PERIOD 16
#endif
static_assert(MK64_3DS_PERF_DETAIL_PERIOD >= 0, "Detail period must be nonnegative");
namespace {
constexpr std::uint32_t kDetailPeriod = MK64_3DS_PERF_DETAIL_PERIOD;
struct TraceStorage {
#ifdef MK64_3DS_AUTODUMP_LAB
    mk64_3ds::PerformanceHistory<1024> history;
#else
    mk64_3ds::PerformanceHistory<256> history;
#endif
    mk64_3ds::PerformanceHistory<256> raceStart;
};
static_assert(sizeof(TraceStorage) < 768 * 1024, "Review the separate trace memory budget");
constexpr std::size_t kLinearStorageBytes = (sizeof(TraceStorage) + 127) & ~std::size_t(127);
TraceStorage* sStorage = nullptr;
bool sLinearStorage = false, sTickOpen = false, sWriting = false;
bool sRaceStartSeen = false, sCaptureRaceStart = false, sRaceActive = false;
#define MK64_PERF_COUNT(name) +1
constexpr std::size_t kFieldCount = 0 MK64_PERF_FIELDS(MK64_PERF_COUNT);
#undef MK64_PERF_COUNT
struct RaceSummary {
    std::uint32_t ticks = 0, drawn = 0, presents = 0, detailed = 0, detailedDrawn = 0;
    std::uint64_t elapsedUs = 0;
    std::array<std::uint64_t, kFieldCount> sum{}, drawnSum{};
    std::array<std::uint32_t, kFieldCount> max{};
} sRaceSummary;
mk64_3ds::PerformanceTick sCurrent;
std::uint64_t sPreviousStart = 0;
std::uint32_t sTick = 0, sEpoch = 0, sGeneration = 0, sCourse = UINT32_MAX;
std::uint32_t sResumeReason = 0, sExcludedUs = 0;
// Only the owner thread touches local counters. Worker completions still use
// atomic cumulative publication; no worker ever writes a live trace row.
enum Counter { Reads, Bytes, ReadUs, Flushes, FlushUs, IoCalls, IoBytes, IoUs, CounterCount };
std::array<std::uint32_t, CounterCount> sLocal{}, sBegin{};
std::array<std::atomic<std::uint32_t>, CounterCount> sRemote{};
std::atomic<void*> sOwnerThread{nullptr};
void Add(Counter counter, std::uint32_t value) {
    if (getThreadLocalStorage() == sOwnerThread.load(std::memory_order_acquire)) sLocal[counter] += value;
    else sRemote[counter].fetch_add(value, std::memory_order_relaxed);
}
std::uint32_t Total(Counter counter) { return sLocal[counter] + sRemote[counter].load(std::memory_order_relaxed); }
std::uint32_t Bounded(std::uint64_t value) { return static_cast<std::uint32_t>(std::min<std::uint64_t>(value, UINT32_MAX)); }
struct Section { Mk64PerfGameSection3DS section; std::uint32_t token; };
std::array<Section, 32> sSections{};
unsigned sDepth = 0;
std::uint32_t sSectionToken = 0;
std::uint64_t sSectionStart = 0;
std::uint32_t& SectionValue(Mk64PerfGameSection3DS section) {
    switch (section) {
        case MK64_PERF_GAME_PHYSICS: return sCurrent.physics_us;
        case MK64_PERF_GAME_COLLISION: return sCurrent.collision_us;
        case MK64_PERF_GAME_PLAYERS: return sCurrent.players_us;
        case MK64_PERF_GAME_ACTORS: return sCurrent.actors_us;
        case MK64_PERF_GAME_CAMERA: return sCurrent.camera_us;
        default: return sCurrent.visual_us;
    }
}
void ChargeSection(std::uint64_t now) {
    if (sDepth) {
        auto& value = SectionValue(sSections[sDepth - 1].section);
        value = Bounded(std::uint64_t(value) + now - sSectionStart);
    }
    sSectionStart = now;
}
bool DetailedField(std::size_t offset) {
    switch (offset) {
#define MK64_DETAIL_OFFSET(name) case offsetof(mk64_3ds::PerformanceTick, name): return true;
        MK64_PERF_DETAIL_FIELDS(MK64_DETAIL_OFFSET)
#undef MK64_DETAIL_OFFSET
        default: return false;
    }
}
// Freeze the completed histories in place rather than allocating a second
// large copy during an SD export. Main-thread producers reject reentrant calls
// while this guard owns them. Small cumulative metadata is copied separately.
struct ExportGuard {
    ExportGuard() { sWriting = true; }
    ~ExportGuard() { sWriting = false; }
};
class BufferedOutput {
  public:
    explicit BufferedOutput(const char* path) : target(path) {
        const int length = std::snprintf(temporary, sizeof(temporary), "%s.tmp", path);
        if (length < 0 || static_cast<std::size_t>(length) >= sizeof(temporary)) return;
        file = std::fopen(temporary, "wb");
        if (file && std::setvbuf(file, buffer, _IOFBF, sizeof(buffer)) != 0) {
            std::fclose(file); file = nullptr;
        }
    }
    ~BufferedOutput() { if (file) std::fclose(file); }
    FILE* Get() const { return file; }
    bool Finish() {
        if (!file) return false;
        const bool ok = std::ferror(file) == 0;
        const int closed = std::fclose(file); file = nullptr;
        // Failed writes keep their .tmp evidence and never replace a complete file.
        return ok && closed == 0 && std::rename(temporary, target) == 0;
    }
  private:
    const char* target;
    char temporary[288]{};
    char buffer[16 * 1024];
    FILE* file = nullptr;
};
bool Path(char* output, std::size_t size, const char* directory, const char* name) {
    const int length = std::snprintf(output, size, "%s/%s", directory, name);
    return length >= 0 && static_cast<std::size_t>(length) < size;
}
}
namespace mk64_3ds {
bool PerformanceInitialize() {
    if (sStorage) return true;
    sOwnerThread.store(getThreadLocalStorage(), std::memory_order_release);
    void* memory = nullptr;
    const std::size_t available = linearSpaceFree();
    if (available >= kLinearStorageBytes + kPerformanceLinearReserve) {
        memory = linearAlloc(kLinearStorageBytes);
        // Enforce the floor after allocator overhead/fragmentation as well.
        if (memory && linearSpaceFree() < kPerformanceLinearReserve) { linearFree(memory); memory = nullptr; }
    }
    sLinearStorage = memory != nullptr;
    // Keep full capacities if linear space is unavailable/fragmented. The caller
    // reserves this before archive admission, so ordinary fallback is charged.
    if (!memory) memory = std::malloc(sizeof(TraceStorage));
    if (!memory) return false;
    sStorage = new (memory) TraceStorage{};
    return true;
}
void PerformanceShutdown() {
    if (sWriting) return;
    PerformanceAbandon();
    if (sStorage) {
        sStorage->~TraceStorage();
        if (sLinearStorage) linearFree(sStorage); else std::free(sStorage);
    }
    sStorage = nullptr;
    sLinearStorage = false;
    sRaceStartSeen = sCaptureRaceStart = sRaceActive = false;
    sRaceSummary = {};
    sPreviousStart = 0;
}
const char* PerformanceStorageKind() { return !sStorage ? "unavailable" : sLinearStorage ? "linear" : "ordinary-fallback"; }
std::size_t PerformanceStorageBytes() { return sStorage ? sizeof(TraceStorage) : 0; }
std::size_t PerformanceStorageReservedBytes() { return sStorage ? (sLinearStorage ? kLinearStorageBytes : sizeof(TraceStorage)) : 0; }
std::uint64_t PerformanceNow() {
    static_assert(SYSCLOCK_ARM11 == 268111856U, "Re-derive the tick conversion constant");
    // Fixed-point conversion avoids ARM11 64-bit division; error ~0.02 ppm.
    constexpr std::uint32_t factor = 16019311U;
    const std::uint64_t ticks = svcGetSystemTick();
    return (ticks >> 32U) * factor + ((std::uint64_t(static_cast<std::uint32_t>(ticks)) * factor) >> 32U);
}
PerformanceTick& PerformanceCurrent() { return sCurrent; }
bool PerformanceDetailsEnabled() { return sTickOpen && !sWriting && sCurrent.detail_sampled != 0; }
bool PerformanceTimerEnabled(const std::uint32_t* output) {
    if (!sTickOpen || sWriting) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(output);
    const auto base = reinterpret_cast<std::uintptr_t>(&sCurrent);
    return address < base || !DetailedField(address - base) || sCurrent.detail_sampled != 0;
}
void PerformanceBegin() {
    if (sWriting || !sStorage) return;
    sCurrent = {};
    sCurrent.start_us = PerformanceNow();
    sCurrent.tick = ++sTick;
    sCurrent.epoch = sEpoch;
    sCurrent.detail_period = kDetailPeriod;
    // Use a nonzero divisor even for the compile-time coarse-only configuration.
    sCurrent.detail_sampled = kDetailPeriod != 0 && (sTick - 1) % (kDetailPeriod ? kDetailPeriod : 1) == 0;
    sCurrent.resume_reason = sResumeReason; sResumeReason = 0;
    sCurrent.excluded_us = sExcludedUs; sExcludedUs = 0;
    sCurrent.course_generation = sGeneration; sCurrent.course_id = sCourse;
    sCurrent.interval_us = sPreviousStart ? Bounded(sCurrent.start_us - sPreviousStart) : 0;
    sPreviousStart = sCurrent.start_us;
    for (unsigned i = 0; i < CounterCount; ++i) sBegin[i] = Total(static_cast<Counter>(i));
    sDepth = 0;
    sTickOpen = true;
}
void PerformanceEnd() {
    if (!sTickOpen || sWriting || !sStorage) return;
    const auto now = PerformanceNow();
    if (sDepth) { ChargeSection(now); ++sCurrent.leaf_faults; sDepth = 0; }
    sCurrent.total_us = Bounded(now - sCurrent.start_us);
    sCurrent.archive_io_calls = Total(IoCalls) - sBegin[IoCalls];
    sCurrent.archive_io_bytes = Total(IoBytes) - sBegin[IoBytes];
    sCurrent.archive_io_us = Total(IoUs) - sBegin[IoUs];
    sCurrent.resource_reads = Total(Reads) - sBegin[Reads];
    sCurrent.resource_bytes = Total(Bytes) - sBegin[Bytes];
    sCurrent.resource_read_us = Total(ReadUs) - sBegin[ReadUs];
    sCurrent.log_flushes = Total(Flushes) - sBegin[Flushes];
    sCurrent.log_flush_us = Total(FlushUs) - sBegin[FlushUs];
    if (!sCurrent.detail_sampled) {
#define MK64_CLEAR_DETAIL(name) sCurrent.name = 0;
        MK64_PERF_DETAIL_FIELDS(MK64_CLEAR_DETAIL)
#undef MK64_CLEAR_DETAIL
    }
    sStorage->history.Push(sCurrent);
    if (sRaceActive && !sCurrent.transition) {
        auto& race = sRaceSummary;
        const bool drawn = sCurrent.presents != 0;
        ++race.ticks; race.drawn += drawn; race.presents += sCurrent.presents;
        race.detailed += sCurrent.detail_sampled;
        race.detailedDrawn += drawn && sCurrent.detail_sampled;
        race.elapsedUs += sCurrent.total_us;
        std::size_t field = 0;
#define MK64_PERF_SUM(name) \
        race.sum[field] += sCurrent.name; \
        if (drawn) race.drawnSum[field] += sCurrent.name; \
        race.max[field] = std::max(race.max[field], sCurrent.name); ++field;
        MK64_PERF_FIELDS(MK64_PERF_SUM)
#undef MK64_PERF_SUM
    }
    if (sCaptureRaceStart) {
        sStorage->raceStart.Push(sCurrent);
        if (sStorage->raceStart.Size() == 256) sCaptureRaceStart = false;
    }
    sTickOpen = false;
}
void PerformanceAbandon() { if (!sWriting) { sTickOpen = false; sDepth = 0; } }
void PerformanceResume(PerformancePause reason, std::uint64_t start) {
    if (sWriting) return;
    PerformanceAbandon();
    ++sEpoch; sPreviousStart = 0;
    sResumeReason = static_cast<std::uint32_t>(reason);
    if (start) sExcludedUs = Bounded(std::uint64_t(sExcludedUs) + PerformanceNow() - start);
}
void PerformanceArchiveRead(std::uint64_t start, std::uint32_t calls, std::uint32_t bytes) {
    if (!calls) return;
    Add(IoCalls, calls); Add(IoBytes, bytes); Add(IoUs, Bounded(PerformanceNow() - start));
}
void PerformanceCourse(std::uint32_t generation, std::uint32_t courseId) {
    if (sWriting) return;
    if (generation != sGeneration || courseId != sCourse) {
        sRaceStartSeen = sCaptureRaceStart = sRaceActive = false;
        sRaceSummary = {};
    }
    sGeneration = generation; sCourse = courseId;
    if (sTickOpen) { sCurrent.course_generation = generation; sCurrent.course_id = courseId; }
}
void PerformanceStateAfter(std::uint32_t gameState, std::uint32_t raceState, std::uint32_t timerMs) {
    if (!sTickOpen || sWriting) return;
    sCurrent.game_state_after = gameState; sCurrent.race_state_after = raceState;
    sCurrent.course_timer_after_ms = timerMs;
    sCurrent.transition = gameState != sCurrent.game_state || raceState != sCurrent.race_state;
}
void PerformanceRaceState(bool racing, std::uint32_t raceState, std::uint32_t timerMs) {
    if (!sTickOpen || sWriting || !sStorage) return;
    sCurrent.race_state = raceState; sCurrent.course_timer_ms = timerMs;
    if (!racing || raceState < 3) { sRaceStartSeen = false; sCaptureRaceStart = false; }
    sRaceActive = racing && raceState == 3;
    if (sRaceActive && !sRaceStartSeen) {
        sRaceStartSeen = sCaptureRaceStart = true;
        sStorage->raceStart.Clear(); sRaceSummary = {};
        const auto& history = sStorage->history;
        const std::size_t begin = history.Size() > 32 ? history.Size() - 32 : 0;
        for (std::size_t i = begin; i < history.Size(); ++i) {
            const auto& row = history.At(i);
            if (row.epoch == sEpoch && row.game_state == sCurrent.game_state &&
                row.course_generation == sGeneration && row.course_id == sCourse) sStorage->raceStart.Push(row);
        }
    }
}
template <std::size_t Capacity>
static bool WriteCsv(const char* path, const PerformanceHistory<Capacity>& history) {
    BufferedOutput output(path); FILE* file = output.Get(); if (!file) return false;
    std::fputs("start_us", file);
#define MK64_PERF_HEADER(name) std::fputs("," #name, file);
    MK64_PERF_FIELDS(MK64_PERF_HEADER)
#undef MK64_PERF_HEADER
    std::fputc('\n', file);
    // Worst-case uint32 columns require eleven bytes each; start_us needs 20.
    std::array<char, 32 + 11 * kFieldCount> line{};
    for (std::size_t i = 0; i < history.Size(); ++i) {
        const auto& row = history.At(i);
        std::size_t position = std::snprintf(line.data(), line.size(), "%llu", static_cast<unsigned long long>(row.start_us));
#define MK64_PERF_VALUE(name) position += std::snprintf(line.data() + position, line.size() - position, ",%lu", static_cast<unsigned long>(row.name));
        MK64_PERF_FIELDS(MK64_PERF_VALUE)
#undef MK64_PERF_VALUE
        line[position++] = '\n';
        if (std::fwrite(line.data(), 1, position, file) != position) return false;
    }
    return output.Finish();
}
static bool WriteRaceSummary(const char* path, const RaceSummary& race) {
    BufferedOutput output(path); FILE* file = output.Get(); if (!file) return false;
    std::fprintf(file, "# Completed driving ticks; transition/export envelopes excluded.\n"
        "# ticks=%lu drawn=%lu presents=%lu elapsed_us=%llu detailed_ticks=%lu detailed_drawn=%lu\n"
        "# Detailed field means use only sampled ticks/draws; sums are not extrapolated.\n",
        static_cast<unsigned long>(race.ticks), static_cast<unsigned long>(race.drawn), static_cast<unsigned long>(race.presents),
        static_cast<unsigned long long>(race.elapsedUs), static_cast<unsigned long>(race.detailed), static_cast<unsigned long>(race.detailedDrawn));
    std::fputs("field,sum,mean_per_tick,mean_per_drawn,max,sampled\n", file);
    std::size_t field = 0;
#define MK64_PERF_SUMMARY(name) { \
        const bool sampled = DetailedField(offsetof(PerformanceTick, name)); \
        const auto ticks = sampled ? race.detailed : race.ticks; \
        const auto drawn = sampled ? race.detailedDrawn : race.drawn; \
        std::fprintf(file, #name ",%llu,%.1f,%.1f,%lu,%u\n", \
            static_cast<unsigned long long>(race.sum[field]), ticks ? race.sum[field] / static_cast<double>(ticks) : 0.0, \
            drawn ? race.drawnSum[field] / static_cast<double>(drawn) : 0.0, static_cast<unsigned long>(race.max[field]), sampled); ++field; }
    MK64_PERF_FIELDS(MK64_PERF_SUMMARY)
#undef MK64_PERF_SUMMARY
    return output.Finish();
}
bool PerformanceWrite(const char* directory) {
    if (!directory || !*directory || !sStorage || sWriting) return false;
    ExportGuard guard;
    const auto& history = sStorage->history;
    const RaceSummary race = sRaceSummary;
    const std::uint32_t generation = sGeneration, course = sCourse;
    char path[256];
    bool ok = Path(path, sizeof(path), directory, "performance.csv") && WriteCsv(path, history);
    ok &= Path(path, sizeof(path), directory, "race-start.csv") && WriteCsv(path, sStorage->raceStart);
    ok &= Path(path, sizeof(path), directory, "race-summary.csv") && WriteRaceSummary(path, race);
    if (!Path(path, sizeof(path), directory, "performance.txt")) return false;
    BufferedOutput output(path); FILE* file = output.Get(); if (!file) return false;
    std::fprintf(file,
        "Performance trace v11; latest %lu completed outer ticks; storage=%s payload_bytes=%lu reserved_bytes=%lu linear_reserve=%lu.\n"
        "Storage is budgeted before archive admission; the reserve is a planning floor, not measured peak headroom.\n"
        "Course generation=%lu course_id=%lu; IDs are stock game course IDs.\n"
        "total_us: full outer envelope, including audio ownership, event polling, controller, preparation, game, audio pump and pacing.\n"
        "Intentional export/suspend/terminal envelopes are discarded. excluded_us on the next row reports their measured wall gap when available.\n"
        "resume_reason: 0 none, 1 diagnostic, 2 lab export, 3 suspend, 4 terminal, 5 resource load. Epoch changes at each exclusion; never compute rates across epochs.\n"
        "detail_period=%lu: 0 coarse only, 1 all detailed, N one in N ticks. detail_sampled marks actual detailed coverage.\n"
        "Unsampled vertex_pack/sp_vertex/sp_vertices/sp_tri_calls/flush/tex_import/tex_imports/tex_delete/matrix/matrix_calls/lookup/lookups/timg fields are zero, not measured zero cost.\n"
        "Resource/artwork/audio/fault/presentation counters and coarse timers cover every completed tick. No detailed sums are extrapolated.\n"
        "physics/collision/players/actors/camera/visual_us are exclusive nested main-thread spans; leaf_faults flags unbalanced/overflowed scopes.\n"
        "game_state/race_state/course_timer_ms are pre-iteration; *_after are post-iteration. transition marks state changes and excludes that row from driving totals.\n"
        "presents counts submitted images, not physical scanouts; outer ticks are not the original internal substeps.\n"
        "iteration_us includes graphics_us; graphics_us includes begin_wait/interpreter/hud/submit/interpolation.\n"
        "begin_wait_us is inside interpreter_us; present_wait_us is a subset of Begin and the SDK CPU timer.\n"
        "key_cpu_us/mid_cpu_us exclude only the post-Begin pending swap wait; previous_gpu_us is a previously completed submission.\n"
        "resource_read_us includes read/decompression and may overlap worker work. archive_io counts underlying fread calls/bytes and read-ahead callback time.\n"
        "art_requests/cache_hits/io_us/io_bytes/decode_us cover the separate Improved pack; native_* counters cover native kart paths.\n"
        "log_flush_us credits worker flush completion within the tick; it can overlap main-thread work.\n"
        "Detailed renderer spans overlap each other and their parents; never add all timer columns.\n"
        "pressure bits: 1 no prior image, 2 audio low, 4 slow presentation, 8 resource burst, 16 texture burst, 32 Citro3D busy, 64 recovery.\n"
        "decision_cpu_us/decision_gpu_us/audio_decision are adaptive inputs; zero pressure on non-adaptive/suppressed ticks is not a health verdict.\n"
        "profile: 0 Old / 1 New; distance: 0 Low / 1 Normal / 2 High.\n"
        "interp_result: 0 no attempt, 1 ready, 2 disabled, 3 empty, 4 overflow, 5 camera cut, 6 signature table, 7 sequence table, 8 prepared table, 9 insufficient matches.\n"
        "interp_current/previous count records; matched/total count unique final destinations. interp_flags: 1 current overflow, 2 previous overflow, 4 camera cut.\n"
        "catchup_key keeps the keyframe while omitting a midpoint; sync_grace records an isolated display wait tolerated by the adaptive policy.\n"
        "static_replays/triangles count recorded runs drawn by PICA; static_captures/fallbacks count recorded/interpreted runs.\n"
        "race-start.csv pins the first 256 driving/countdown rows; race-summary.csv retains all completed driving ticks across epochs.\n",
        static_cast<unsigned long>(history.Size()), PerformanceStorageKind(), static_cast<unsigned long>(PerformanceStorageBytes()),
        static_cast<unsigned long>(PerformanceStorageReservedBytes()),
        static_cast<unsigned long>(kPerformanceLinearReserve), static_cast<unsigned long>(generation), static_cast<unsigned long>(course),
        static_cast<unsigned long>(kDetailPeriod));
    std::uint64_t start = 0, end = 0, images = 0, skipped = 0;
    std::uint32_t count = 0, worst = 0;
    const auto epoch = history.Size() ? history.At(history.Size() - 1).epoch : 0;
    for (std::size_t i = 0; i < history.Size(); ++i) {
        const auto& row = history.At(i);
        if (row.epoch != epoch) continue;
        if (!count) start = row.start_us;
        end = row.start_us + row.total_us;
        ++count; images += row.presents; skipped += row.suppressed; worst = std::max(worst, row.total_us);
    }
    std::fprintf(file, "Current epoch %lu: ticks=%lu elapsed_us=%llu submitted=%llu suppressed=%llu worst_tick_us=%lu\n",
        static_cast<unsigned long>(epoch), static_cast<unsigned long>(count), static_cast<unsigned long long>(end - start),
        static_cast<unsigned long long>(images), static_cast<unsigned long long>(skipped), static_cast<unsigned long>(worst));
    if (end > start) std::fprintf(file, "Active-window submitted images/s=%.2f; outer ticks/s=%.2f (not physical scanouts).\n",
        images * 1.0e6 / static_cast<double>(end - start), count * 1.0e6 / static_cast<double>(end - start));
    return output.Finish() && ok;
}
}
extern "C" std::uint64_t Mk64Perf3DSResourceStart() { return mk64_3ds::PerformanceNow(); }
extern "C" void Mk64Perf3DSResourceRead(std::uint64_t start, std::size_t bytes) {
    Add(Reads, 1); Add(Bytes, static_cast<std::uint32_t>(bytes)); Add(ReadUs, Bounded(mk64_3ds::PerformanceNow() - start));
}
extern "C" void Mk64Perf3DSLogFlush(std::uint64_t start) {
    Add(Flushes, 1); Add(FlushUs, Bounded(mk64_3ds::PerformanceNow() - start));
}
extern "C" void Mk64Perf3DSPreparedPaletteHit() { ++mk64_3ds::PerformanceCurrent().native_palette_hits; }
extern "C" std::uint32_t Mk64Perf3DSGameSectionBegin(Mk64PerfGameSection3DS section) {
    if (!sTickOpen || sWriting) return 0;
    if (static_cast<unsigned>(section) >= MK64_PERF_GAME_SECTION_COUNT || sDepth == sSections.size()) {
        ++sCurrent.leaf_faults; return 0;
    }
    ChargeSection(mk64_3ds::PerformanceNow());
    if (!++sSectionToken) ++sSectionToken;
    sSections[sDepth++] = {section, sSectionToken};
    return sSectionToken;
}
extern "C" void Mk64Perf3DSGameSectionEnd(std::uint32_t token) {
    if (!token || !sTickOpen || sWriting) return;
    if (!sDepth || sSections[sDepth - 1].token != token) {
        ++sCurrent.leaf_faults; sDepth = 0; return;
    }
    ChargeSection(mk64_3ds::PerformanceNow()); --sDepth;
}
