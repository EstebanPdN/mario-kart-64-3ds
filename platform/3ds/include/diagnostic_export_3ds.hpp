#pragma once
#include "audio_runtime_3ds.h"
#include "bottom_ui_3ds.h"
#include "game_runtime_3ds.h"
#include "performance_trace_3ds.hpp"
#include "race_clock_3ds.hpp"

namespace mk64_3ds {
// Main-thread-only export transaction. Audio pause drains the producer before
// any write. Completed trace storage stays immutable; incomplete outer work is
// excluded. The caller must reset its pacer and skip gameplay on return.
class DiagnosticExportPause {
  public:
    explicit DiagnosticExportPause(PerformancePause reason = PerformancePause::LabExport)
        : start(PerformanceNow()), reason(reason) {
        Mk64RaceClock3DSFreeze();
        Mk64GameAudio3DSSetPaused(true);
        PerformanceAbandon();
    }
    ~DiagnosticExportPause() {
        Mk64RaceClock3DSFreeze();
        Mk64Graphics3DSResumeAfterDiagnosticPause();
        Mk64BottomUI3DSResetFps();
        PerformanceResume(reason, start);
        if (resumeAudio) Mk64GameAudio3DSSetPaused(false);
    }
    void KeepPaused() { resumeAudio = false; }
    DiagnosticExportPause(const DiagnosticExportPause&) = delete;
    DiagnosticExportPause& operator=(const DiagnosticExportPause&) = delete;
  private:
    std::uint64_t start;
    PerformancePause reason;
    bool resumeAudio = true;
};
}
