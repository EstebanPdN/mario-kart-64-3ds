#ifdef MK64_3DS_AUTODUMP_LAB
#include "autodump_lab_3ds.h"
#include "autodump_schedule_3ds.hpp"
#include "auto_mode_3ds.h"
#include "audio_runtime_3ds.h"
#include "bottom_ui_3ds.h"
#include "diagnostics_3ds.h"
#include "improved_texture_pack_3ds.hpp"
#include "improved_resources_3ds.h"
#include "settings_3ds.h"
#include "performance_trace_3ds.hpp"
#include "diagnostic_export_3ds.hpp"
#include "engine/World.h"
#include "engine/Cup.h"
#include <cstdio>
#include <cstring>
#include <sys/statvfs.h>
#include <sys/stat.h>
extern "C" {
#include "main.h"
#include "menus.h"
#include "menu_items.h"
#include "racing/race_logic.h"
std::uint64_t svcGetSystemTick(void);
std::int32_t svcSleepThread(std::int64_t);
void TrackBrowser_SetTrackFromCup();
void Mk64Auto3DSLabEnable();
}
namespace {
mk64_3ds::AutoDumpSchedule sPlan;
std::uint32_t sSeed;
bool sChanging = false, sTerminal = false;
std::uint64_t sRaceWallStart = 0;
std::uint32_t sLastCaptureGameMs = 0;
const char* sError = nullptr;
std::uint32_t GameMs() {
    return gCourseTimer > 0 ? static_cast<std::uint32_t>(gCourseTimer * 1000) : 0;
}
constexpr const char* kTracks[] = {
    "mk:luigi_raceway", "mk:moo_moo_farm", "mk:koopa_troopa_beach", "mk:kalimari_desert",
    "mk:toads_turnpike", "mk:frappe_snowland", "mk:choco_mountain", "mk:mario_raceway",
    "mk:wario_stadium", "mk:sherbet_land", "mk:royal_raceway", "mk:bowsers_castle",
    "mk:dk_jungle", "mk:yoshi_valley", "mk:banshee_boardwalk", "mk:rainbow_road"
};
static_assert(sizeof(kTracks) / sizeof(*kTracks) == 16);
std::uint64_t FreeBytes() {
    struct statvfs info{};
    if (statvfs("sdmc:/3ds/MK64", &info)) return 0;
    return std::uint64_t(info.f_bavail) * info.f_frsize;
}
void Fail(const char* reason) { sError = reason; sPlan.failed = true; }
bool Journal(const char* state, const char* dump = "") {
    char path[256];
    std::snprintf(path, sizeof(path), "%s/session.txt", Mk64Diagnostics3DSLabDirectory());
    FILE* file = std::fopen(path, "a");
    if (!file) return false;
    std::fprintf(file, "state=%s seed=%lu race=%u/6 capture=%u/3 complete=%u/18 track=%s game_ms=%.0f free_bytes=%llu dump=%s\n",
        state, static_cast<unsigned long>(sSeed), sPlan.race < 6 ? sPlan.race + 1 : 6, sPlan.captured, sPlan.total,
        kTracks[sPlan.courses[sPlan.race < 6 ? sPlan.race : 5]], gCourseTimer * 1000.0f,
        static_cast<unsigned long long>(FreeBytes()), dump);
    const bool ok = std::ferror(file) == 0;
    return std::fclose(file) == 0 && ok;
}
void BeginRace() {
    static Cup cup("diagnostic-lab", "Diagnostic Lab", std::vector<std::string>(4, kTracks[0]));
    cup.mTracks.assign(4, kTracks[sPlan.courses[sPlan.race]]);
    auto* world = GetWorld(); world->SetCupIndex(MUSHROOM_CUP); world->SetCurrentCup(&cup);
    gPlayerCount = gPlayerCountSelection1 = 1; gScreenModeSelection = SCREEN_MODE_1P;
    gModeSelection = GRAND_PRIX; gCCSelection = CC_50; gDemoMode = 0; gIsGamePaused = 0;
    gCharacterSelections[0] = MARIO; gCupSelection = MUSHROOM_CUP;
    gCourseIndexInCup = 0; gIsMirrorMode = 0;
    TrackBrowser_SetTrackFromCup(); Mk64Auto3DSLabEnable();
    gGamestateNext = RACING; sChanging = false; sRaceWallStart = osGetTime();
    sLastCaptureGameMs = 0;
    if (!Journal("RACE_REQUESTED")) Fail("Cannot write laboratory session log");
}
void FinalizeTerminal() {
    if (sTerminal) return;
    // This is called only while the audio/clock export transaction is held.
    if (!sPlan.failed && (!Mk64Diagnostics3DSLabFlush() || !Journal("FINISH")))
        Fail("Cannot save final session status");
    sTerminal = true;
}
}
extern "C" bool Mk64Lab3DSBoot() {
    if (!mk64_3ds::PerformanceStorageBytes()) {
        Fail("Cannot allocate complete laboratory trace storage"); return false;
    }
    sSeed = static_cast<std::uint32_t>(svcGetSystemTick()) ^ static_cast<std::uint32_t>(osGetTime());
    sPlan.Shuffle(sSeed);
    if (!mk64_3ds::improved::LocalPack().Load("@hint_option",87,29) ||
        !mk64_3ds::improved::LocalPack().Load("@hint_random",192,29)) {
        Fail("Required bundled improved artwork is unavailable"); return false;
    }
    // A conservative complete-run allowance; the per-dump guard also runs
    // before each write. Existing dumps and game data are never removed.
    if (FreeBytes() < 5ULL * 1024 * 1024 * 1024) {
        Fail("Laboratory needs at least 5 GiB free on SD"); return false;
    }
    char path[256];std::snprintf(path,sizeof(path),"%s/plan.txt",Mk64Diagnostics3DSLabDirectory());
    FILE* file=std::fopen(path,"w");
    if (!file) { Fail("Cannot save laboratory plan"); return false; }
    std::fprintf(file,"Build: %s\nSeed: %lu\n6 unique random tracks; 50cc AUTO, 8 CPU karts.\n3 complete dumps per track: first two at scene appearance/15/30 game seconds; remaining four at 15/30/45 game seconds.\nAdvance after 5 game seconds following the third successful dump; no lap-completion wait.\nSaved display/audio/texture settings retained. Capture and final-summary writes are paused and excluded from driving timing; transition journaling remains in controller envelopes.\n",MK64_3DS_VERSION,static_cast<unsigned long>(sSeed));
    for(unsigned i=0;i<6;++i)std::fprintf(file,"%u %s captures=%s\n",i+1,kTracks[sPlan.courses[i]],i<mk64_3ds::AutoDumpSchedule::EarlyRaces?"appearance/15/30":"15/30/45");
    const bool ok=std::ferror(file)==0;const int closed=std::fclose(file);
    if (!ok || closed) { Fail("Cannot finish laboratory plan"); return false; }
    BeginRace(); return !sPlan.failed;
}
extern "C" int Mk64Lab3DSTick() {
    if (sPlan.failed || sPlan.Finished()) {
        if (!sTerminal) {
            mk64_3ds::DiagnosticExportPause pause;
            FinalizeTerminal();
            pause.KeepPaused();
        }
        Mk64BottomUI3DSShowProgress(sPlan.failed ? "LAB ERROR" : "FINISH",
            sPlan.failed ? (sError ? sError : "Dump write failed; keep partial data") : "18 / 18 DUMPS SAVED - START TO EXIT", sPlan.failed ? 0 : 100);
        Mk64DiagnosticsInput3DS input{};
        if (Mk64Diagnostics3DSConsumeInput(&input) && (input.downMask & (1U << 3))) return 2;
        svcSleepThread(16000000LL); return 1;
    }
    if (sChanging) {
        if (gGamestate != RACING) BeginRace();
        return sPlan.failed ? MK64_LAB_TERMINAL : MK64_LAB_ITERATE;
    }
    // A bad/blocked course must leave an explicit error rather than waiting
    // forever or falsely reporting FINISH. Dumps on slow SD are outside this
    // deadline because the timestamp is refreshed after every complete write.
    if (osGetTime() - sRaceWallStart > 15ULL * 60 * 1000) {
        Fail("Race timeout; keep the laboratory folder for diagnosis"); return 1;
    }
    if (gGamestate != RACING || gRaceState < RACE_SETUP || gIsGamePaused) return 0;
    // The first two tracks also capture the newly rendered starting grid,
    // before the countdown ends. Later captures use the driving clock.
    if (gRaceState < RACE_IN_PROGRESS && !(sPlan.HasStartCapture() && sPlan.captured == 0)) return 0;
    const unsigned gameMs = GameMs();
    if (sPlan.Due(gameMs)) {
        if (FreeBytes() < 384ULL * 1024 * 1024) { Fail("SD space too low; partial data preserved"); return 1; }
        if (Mk64Diagnostics3DSRequestLabDump()) sPlan.pending = true;
    }
    if (!sPlan.pending && sPlan.captured == sPlan.Captures &&
        gameMs >= sLastCaptureGameMs && gameMs - sLastCaptureGameMs >= sPlan.PostCaptureMs) {
        // Save the measured segment and final rolling window at its endpoint,
        // then unload the track without waiting for any lap to finish.
        mk64_3ds::DiagnosticExportPause pause;
        char finalDirectory[256];
        std::snprintf(finalDirectory,sizeof(finalDirectory),"%s/race-%02u-final",
            Mk64Diagnostics3DSLabDirectory(),sPlan.race+1);
        if (mkdir(finalDirectory,0777) || !mk64_3ds::PerformanceWrite(finalDirectory) ||
            !Mk64Improved3DSWriteHistory(finalDirectory)) {
            Fail("Cannot save final race timing summary"); pause.KeepPaused(); return MK64_LAB_TERMINAL;
        }
        if (!Journal("TRACK_CAPTURE_COMPLETE")) {
            Fail("Cannot save track completion"); pause.KeepPaused(); return MK64_LAB_TERMINAL;
        }
        if (!sPlan.NextRace()) {
            Fail("Invalid laboratory completion state"); pause.KeepPaused(); return MK64_LAB_TERMINAL;
        }
        if (sPlan.Finished()) {
            FinalizeTerminal(); pause.KeepPaused(); return MK64_LAB_TERMINAL;
        }
        sChanging = true; gGamestateNext = MAIN_MENU_FROM_QUIT;
        return MK64_LAB_EXPORTED;
    }
    return 0;
}
extern "C" bool Mk64Lab3DSWriteContext(const char* directory) {
    char path[256]; std::snprintf(path,sizeof(path),"%s/lab-context.txt",directory);
    FILE* file=std::fopen(path,"w"); if(!file)return false;
    std::fprintf(file,"Build: %s\nSeed: %lu\nRace: %u/6\nCapture: %u/3\nCapture kind: %s\nRace state: %d\nTrack: %s\nCourse timer: %.3f\nTexture mode: %d\nStatic meshes: %s\nFree SD bytes before dump: %llu\nFull application RAM, screen captures, display list, logs, timing/opcode/audio/GPU counters included.\nDiagnostic write time is excluded from game timing.\n",MK64_3DS_VERSION,static_cast<unsigned long>(sSeed),sPlan.race+1,sPlan.captured+1,sPlan.HasStartCapture()&&sPlan.captured==0?"starting-grid":"driving",int(gRaceState),kTracks[sPlan.courses[sPlan.race]],gCourseTimer,int(Mk64Settings3DSGetUiTextures()),Mk64Settings3DSGetStaticMeshes()?"on":"off",static_cast<unsigned long long>(FreeBytes()));
    const bool ok=std::ferror(file)==0;return std::fclose(file)==0&&ok;
}
extern "C" void Mk64Lab3DSDumpComplete(const char* directory, bool success) {
    if (!sPlan.Complete(success)) { Fail("Incomplete dump; partial files preserved"); return; }
    sRaceWallStart=osGetTime();
    sLastCaptureGameMs=GameMs();
    if (!Journal("DUMP_COMPLETE",directory)) Fail("Cannot save dump completion");
}
#endif
