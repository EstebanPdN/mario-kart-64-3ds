#pragma once

#include <cstdint>

namespace mk64_3ds {
enum class InterpolationResult : std::uint32_t {
    NotAttempted, Ready, Disabled, EmptyRecording, RecordingOverflow,
    CameraCut, SignatureTableFull, SequenceTableFull, PreparedTableFull,
    InsufficientMatches
};
struct InterpolationDiagnostic {
    InterpolationResult result = InterpolationResult::NotAttempted;
    std::uint32_t current = 0, previous = 0, matched = 0, total = 0, flags = 0;
};
const InterpolationDiagnostic& InterpolationLastDiagnostic();
bool InterpolationWriteDiagnostic(const char* directory);
}
