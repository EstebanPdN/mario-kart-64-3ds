#include "port/interpolation/FrameInterpolation.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <unordered_map>

// The desktop recorder builds a deeply nested collection tree every frame.
// That allocator-heavy implementation exhausted the Old 3DS during v0.11.
// The 3DS port only needs the final matrices consumed by Fast3D, so keep two
// bounded recordings and match them by stable render scopes. Their storage is
// allocated once only when the New-3DS 400 px presentation mode is active, so
// Old 3DS does not pay the roughly 330 KiB BSS cost. The frame path itself
// performs no node allocations.
namespace {

constexpr size_t kMaxRecordedMatrices = 2048;
constexpr size_t kPreparedTableCapacity = 4096;
constexpr size_t kSignatureTableCapacity = 4096;
constexpr size_t kChildOccurrenceCapacity = 4096;
constexpr size_t kMaxScopeDepth = 64;
static_assert((kPreparedTableCapacity & (kPreparedTableCapacity - 1U)) == 0);
static_assert((kSignatureTableCapacity & (kSignatureTableCapacity - 1U)) == 0);
static_assert((kChildOccurrenceCapacity & (kChildOccurrenceCapacity - 1U)) == 0);
static_assert(kMaxRecordedMatrices <= UINT16_MAX);
constexpr uint16_t kInvalidRecordIndex = UINT16_MAX;
constexpr size_t kMinimumMatchPercent = 70;

enum class MatrixKind : uint8_t {
    Converted,
    MatrixStack,
    Skin,
};

struct MatrixRecord {
    Mtx* destination = nullptr;
    MtxF value = {};
    uintptr_t signature = 0;
    uint32_t sequenceHash = 0;
    MatrixKind kind = MatrixKind::Converted;
};

struct MatrixFrame {
    std::array<MatrixRecord, kMaxRecordedMatrices> records = {};
    size_t count = 0;
    bool overflowed = false;
    bool cameraCut = false;
};

struct ScopeState {
    uint32_t hash = 0;
    uint32_t matrixOrdinal = 0;
    uint32_t markerHash = 0;
    uint32_t markerOrdinal = 0;
};

struct SignatureSlot {
    uint32_t signature = 0;
    uint16_t recordIndex = 0;
    MatrixKind kind = MatrixKind::Converted;
    uint8_t state = 0; // 0 empty, 1 unique, 2 ambiguous, 3 consumed
};

struct ChildOccurrenceSlot {
    uint32_t parentHash = 0;
    uintptr_t label = 0;
    uintptr_t tag = 0;
    uint32_t count = 0;
    uint16_t previousMatrixCount = 0;
    uint16_t currentMatrixCount = 0;
    bool occupied = false;
};

struct RecorderStorage {
    std::array<MatrixFrame, 2> frames = {};
    std::array<Mtx*, kPreparedTableCapacity> preparedKeys = {};
    std::array<uint16_t, kPreparedTableCapacity> preparedCurrentIndices = {};
    std::array<uint16_t, kPreparedTableCapacity> preparedPreviousIndices = {};
    std::array<SignatureSlot, kSignatureTableCapacity> previousSignatures = {};
    std::array<ChildOccurrenceSlot, kChildOccurrenceCapacity> childOccurrences = {};
    std::array<ScopeState, kMaxScopeDepth> scopes = {};
};

std::unique_ptr<RecorderStorage> sStorage;
float sPreparedStep = 0.5f;
bool sPreparedActive = false;
unsigned sCurrentFrame = 0;
unsigned sPreviousFrame = 1;
bool sEnabled = false;
bool sRecording = false;
bool sAllowCurrentFrame = true;
uint32_t sCameraEpoch = 0;
size_t sScopeDepth = 0;
uint32_t sPrepareAttempts = 0;
uint32_t sPreparedFrames = 0;
uint32_t sRetainedFrames = 0;
uint32_t sLastMatchedMatrices = 0;
uint32_t sLastTotalMatrices = 0;

extern "C" Mat4* gInterpolationMatrix;

uint32_t MixHash(uint32_t hash, uint32_t value) {
    // Murmur3's integer finalizer gives pointer/tag/ordinal combinations good
    // diffusion on the 3DS' 32-bit address space without any allocation.
    value ^= value >> 16U;
    value *= 0x85EBCA6BU;
    value ^= value >> 13U;
    value *= 0xC2B2AE35U;
    value ^= value >> 16U;
    hash ^= value + 0x9E3779B9U + (hash << 6U) + (hash >> 2U);
    return hash;
}

ScopeState* CurrentScope() {
    if (sStorage == nullptr || sScopeDepth == 0 || sScopeDepth > sStorage->scopes.size()) {
        return nullptr;
    }
    return &sStorage->scopes[sScopeDepth - 1U];
}

uintptr_t MatrixSignature(MatrixKind kind, uintptr_t callsite, uint32_t* sequenceHash) {
    ScopeState* scope = CurrentScope();
    if (scope == nullptr) {
        if (sStorage != nullptr) sStorage->frames[sCurrentFrame].overflowed = true;
        if (sequenceHash != nullptr) *sequenceHash = 0;
        return 0;
    }
    uint32_t hash = MixHash(scope->hash, scope->markerHash);
    if (sequenceHash != nullptr) *sequenceHash = hash;
    hash = MixHash(hash, static_cast<uint32_t>(kind));
    hash = MixHash(hash, static_cast<uint32_t>(callsite));
    hash = MixHash(hash, scope->matrixOrdinal++);
    return static_cast<uintptr_t>(hash);
}

void AppendMatrix(Mtx* destination, const MtxF& value, MatrixKind kind, uintptr_t callsite) {
    if (!sEnabled || !sRecording) {
        return;
    }
    MatrixFrame& frame = sStorage->frames[sCurrentFrame];
    uint32_t sequenceHash = 0;
    const uintptr_t signature = MatrixSignature(kind, callsite, &sequenceHash);
    if (destination == nullptr) {
        frame.overflowed = true;
        return;
    }
    if (frame.count >= frame.records.size()) {
        frame.overflowed = true;
        return;
    }
    frame.records[frame.count++] = { destination, value, signature, sequenceHash, kind };
}

MtxF InterpolateMatrix(const MtxF& previous, const MtxF& current, float step) {
    MtxF result = {};
    const float oldWeight = 1.0f - step;
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            result.mf[row][column] =
                previous.mf[row][column] * oldWeight + current.mf[row][column] * step;
            if (!std::isfinite(result.mf[row][column])) return current;
        }
    }

    // Direct matrix lerp is intentionally cheap, but rotations near 180
    // degrees can collapse a basis into a flat "paper" matrix. Snap only
    // those degenerate blends to the current key matrix; ordinary movement
    // and rotation continue to interpolate.
    for (size_t axis = 0; axis < 3; ++axis) {
        float previousLengthSquared = 0.0f;
        float currentLengthSquared = 0.0f;
        float resultLengthSquared = 0.0f;
        for (size_t component = 0; component < 3; ++component) {
            previousLengthSquared += previous.mf[axis][component] * previous.mf[axis][component];
            currentLengthSquared += current.mf[axis][component] * current.mf[axis][component];
            resultLengthSquared += result.mf[axis][component] * result.mf[axis][component];
        }
        const float reference = std::min(previousLengthSquared, currentLengthSquared);
        if (reference > 1.0e-8f && resultLengthSquared < reference * 0.04f) {
            return current;
        }
    }
    return result;
}

size_t PreparedIndex(const Mtx* destination) {
    const uintptr_t word = reinterpret_cast<uintptr_t>(destination) >> 4U;
    return static_cast<size_t>((word * uintptr_t{2654435761U}) &
                               (kPreparedTableCapacity - 1U));
}

bool SetPrepared(Mtx* destination, size_t currentIndex, size_t previousIndex) {
    if (destination == nullptr) return false;
    size_t index = PreparedIndex(destination);
    for (size_t probe = 0; probe < kPreparedTableCapacity; ++probe) {
        if (sStorage->preparedKeys[index] == nullptr) {
            sStorage->preparedKeys[index] = destination;
            sStorage->preparedCurrentIndices[index] = static_cast<uint16_t>(currentIndex);
            sStorage->preparedPreviousIndices[index] = static_cast<uint16_t>(previousIndex);
            return true;
        }
        if (sStorage->preparedKeys[index] == destination) {
            // Fast3D consumes the final matrix written to a reused destination.
            // Match the desktop replacement map's operator[] (last write wins).
            sStorage->preparedCurrentIndices[index] = static_cast<uint16_t>(currentIndex);
            sStorage->preparedPreviousIndices[index] = static_cast<uint16_t>(previousIndex);
            return true;
        }
        index = (index + 1U) & (kPreparedTableCapacity - 1U);
    }
    return false;
}

size_t SignatureIndex(uint32_t signature, MatrixKind kind) {
    const uint32_t hash = MixHash(signature, static_cast<uint32_t>(kind));
    return static_cast<size_t>(hash & (kSignatureTableCapacity - 1U));
}

bool InsertPreviousSignature(const MatrixRecord& record, size_t recordIndex) {
    size_t index = SignatureIndex(static_cast<uint32_t>(record.signature), record.kind);
    for (size_t probe = 0; probe < kSignatureTableCapacity; ++probe) {
        SignatureSlot& slot = sStorage->previousSignatures[index];
        if (slot.state == 0) {
            slot.signature = static_cast<uint32_t>(record.signature);
            slot.kind = record.kind;
            slot.recordIndex = static_cast<uint16_t>(recordIndex);
            slot.state = 1;
            return true;
        }
        if (slot.signature == static_cast<uint32_t>(record.signature) && slot.kind == record.kind) {
            // A 32-bit identity collision is safer left uninterpolated than
            // paired with an arbitrary actor.
            slot.state = 2;
            return true;
        }
        index = (index + 1U) & (kSignatureTableCapacity - 1U);
    }
    return false;
}

bool TakePreviousSignature(const MatrixRecord& record, size_t* recordIndex) {
    size_t index = SignatureIndex(static_cast<uint32_t>(record.signature), record.kind);
    for (size_t probe = 0; probe < kSignatureTableCapacity; ++probe) {
        SignatureSlot& slot = sStorage->previousSignatures[index];
        if (slot.state == 0) return false;
        if (slot.signature == static_cast<uint32_t>(record.signature) && slot.kind == record.kind) {
            if (slot.state != 1) return false;
            *recordIndex = slot.recordIndex;
            // A second current record with the same 32-bit identity is a hash
            // collision (or otherwise ambiguous), not the same semantic write.
            slot.state = 3;
            return true;
        }
        index = (index + 1U) & (kSignatureTableCapacity - 1U);
    }
    return false;
}

bool NextChildOccurrence(uint32_t parentHash, const void* label, uintptr_t tag,
                         uint32_t* occurrence) {
    const uintptr_t labelWord = reinterpret_cast<uintptr_t>(label);
    uint32_t hash = MixHash(parentHash, static_cast<uint32_t>(labelWord));
    hash = MixHash(hash, static_cast<uint32_t>(tag));
    size_t index = static_cast<size_t>(hash & (kChildOccurrenceCapacity - 1U));
    for (size_t probe = 0; probe < kChildOccurrenceCapacity; ++probe) {
        ChildOccurrenceSlot& slot = sStorage->childOccurrences[index];
        if (!slot.occupied) {
            slot.parentHash = parentHash;
            slot.label = labelWord;
            slot.tag = tag;
            slot.count = 1;
            slot.occupied = true;
            *occurrence = 0;
            return true;
        }
        if (slot.parentHash == parentHash && slot.label == labelWord && slot.tag == tag) {
            *occurrence = slot.count++;
            return true;
        }
        index = (index + 1U) & (kChildOccurrenceCapacity - 1U);
    }
    return false;
}

size_t SequenceCountIndex(uint32_t sequenceHash) {
    return static_cast<size_t>(MixHash(sequenceHash, 0x53434F50U) &
                               (kChildOccurrenceCapacity - 1U));
}

bool IncrementSequenceCount(uint32_t sequenceHash, bool currentFrame) {
    size_t index = SequenceCountIndex(sequenceHash);
    for (size_t probe = 0; probe < kChildOccurrenceCapacity; ++probe) {
        ChildOccurrenceSlot& slot = sStorage->childOccurrences[index];
        if (!slot.occupied) {
            slot.parentHash = sequenceHash;
            slot.occupied = true;
        }
        if (slot.parentHash == sequenceHash) {
            uint16_t& count = currentFrame ? slot.currentMatrixCount
                                           : slot.previousMatrixCount;
            if (count == UINT16_MAX) return false;
            ++count;
            return true;
        }
        index = (index + 1U) & (kChildOccurrenceCapacity - 1U);
    }
    return false;
}

bool SequenceCountsMatch(uint32_t sequenceHash) {
    size_t index = SequenceCountIndex(sequenceHash);
    for (size_t probe = 0; probe < kChildOccurrenceCapacity; ++probe) {
        const ChildOccurrenceSlot& slot = sStorage->childOccurrences[index];
        if (!slot.occupied) return false;
        if (slot.parentHash == sequenceHash) {
            return slot.previousMatrixCount != 0 &&
                   slot.previousMatrixCount == slot.currentMatrixCount;
        }
        index = (index + 1U) & (kChildOccurrenceCapacity - 1U);
    }
    return false;
}

} // namespace

extern "C" void Mk64FrameInterpolation3DSSetEnabled(bool enabled) {
    sStorage.reset();
    if (enabled) {
        sStorage.reset(new (std::nothrow) RecorderStorage{});
    }
    sEnabled = enabled && sStorage != nullptr;
    sRecording = false;
    sCurrentFrame = 0;
    sPreviousFrame = 1;
    sPreparedActive = false;
    sScopeDepth = 0;
    sAllowCurrentFrame = true;
    sPrepareAttempts = 0;
    sPreparedFrames = 0;
    sRetainedFrames = 0;
    sLastMatchedMatrices = 0;
    sLastTotalMatrices = 0;
}

extern "C" bool Mk64FrameInterpolation3DSIsEnabled(void) {
    return sEnabled;
}

std::unordered_map<Mtx*, MtxF> FrameInterpolation_Interpolate(float step) {
    // Desktop callers retain this ABI. The 3DS renderer uses the fixed-table
    // prepare/lookup path below so its 60 Hz midpoint does not allocate an
    // unordered-map node for every recorded matrix on every simulation tick.
    (void)step;
    return {};
}

extern "C" bool Mk64FrameInterpolation3DSPrepare(float step) {
    sPreparedActive = false;
    ++sPrepareAttempts;
    sLastMatchedMatrices = 0;
    sLastTotalMatrices = 0;
    if (!sEnabled || sStorage == nullptr) {
        ++sRetainedFrames;
        return false;
    }
    sStorage->preparedKeys.fill(nullptr);
    sStorage->preparedCurrentIndices.fill(kInvalidRecordIndex);
    sStorage->preparedPreviousIndices.fill(kInvalidRecordIndex);
    sStorage->previousSignatures.fill({});
    // Recording no longer needs the child-occurrence scratch table. Reuse it
    // to compare matrix counts per stable scope/marker sequence without adding
    // another large New-3DS allocation.
    sStorage->childOccurrences.fill({});

    const MatrixFrame& current = sStorage->frames[sCurrentFrame];
    const MatrixFrame& previous = sStorage->frames[sPreviousFrame];
    sLastTotalMatrices = static_cast<uint32_t>(current.count);
    if (current.count == 0 || previous.count == 0 || current.overflowed ||
        previous.overflowed || current.cameraCut) {
        ++sRetainedFrames;
        return false;
    }

    step = std::fmax(0.0f, std::fmin(1.0f, step));
    sPreparedStep = step;
    for (size_t index = 0; index < previous.count; ++index) {
        if (!IncrementSequenceCount(previous.records[index].sequenceHash, false) ||
            !InsertPreviousSignature(previous.records[index], index)) {
            sStorage->preparedKeys.fill(nullptr);
            ++sRetainedFrames;
            return false;
        }
    }
    for (size_t index = 0; index < current.count; ++index) {
        if (!IncrementSequenceCount(current.records[index].sequenceHash, true)) {
            sStorage->preparedKeys.fill(nullptr);
            ++sRetainedFrames;
            return false;
        }
    }

    for (size_t currentIndex = 0; currentIndex < current.count; ++currentIndex) {
        const MatrixRecord& newRecord = current.records[currentIndex];
        size_t previousIndex = kInvalidRecordIndex;
        if (SequenceCountsMatch(newRecord.sequenceHash)) {
            size_t matchedPreviousIndex = 0;
            if (TakePreviousSignature(newRecord, &matchedPreviousIndex)) {
                previousIndex = matchedPreviousIndex;
            }
        }
        // Store matched and unmatched writes alike. A later write to a reused
        // Mtx replaces the earlier state, and the final validity scan below
        // therefore measures actual replacements rather than transient hits.
        if (!SetPrepared(newRecord.destination, currentIndex, previousIndex)) {
            sStorage->preparedKeys.fill(nullptr);
            ++sRetainedFrames;
            return false;
        }
    }

    size_t matched = 0;
    size_t total = 0;
    for (size_t index = 0; index < kPreparedTableCapacity; ++index) {
        if (sStorage->preparedKeys[index] == nullptr) continue;
        ++total;
        if (sStorage->preparedPreviousIndices[index] != kInvalidRecordIndex) ++matched;
    }
    sLastMatchedMatrices = static_cast<uint32_t>(matched);
    sLastTotalMatrices = static_cast<uint32_t>(total);
    if (matched == 0 || total == 0 || matched * 100U < total * kMinimumMatchPercent) {
        sStorage->preparedKeys.fill(nullptr);
        ++sRetainedFrames;
        return false;
    }
    sPreparedActive = true;
    ++sPreparedFrames;
    return true;
}

extern "C" void Mk64FrameInterpolation3DSClearPrepared(void) {
    sPreparedActive = false;
}

extern "C" bool Mk64FrameInterpolation3DSLookup(const Mtx* destination, MtxF* replacement) {
    if (!sPreparedActive || sStorage == nullptr || destination == nullptr || replacement == nullptr) {
        return false;
    }
    size_t index = PreparedIndex(destination);
    for (size_t probe = 0; probe < kPreparedTableCapacity; ++probe) {
        Mtx* key = sStorage->preparedKeys[index];
        if (key == destination) {
            const size_t currentIndex = sStorage->preparedCurrentIndices[index];
            const size_t previousIndex = sStorage->preparedPreviousIndices[index];
            const MatrixFrame& current = sStorage->frames[sCurrentFrame];
            const MatrixFrame& previous = sStorage->frames[sPreviousFrame];
            if (previousIndex == kInvalidRecordIndex || currentIndex >= current.count ||
                previousIndex >= previous.count) {
                return false;
            }
            *replacement = InterpolateMatrix(previous.records[previousIndex].value,
                                             current.records[currentIndex].value,
                                             sPreparedStep);
            return true;
        }
        if (key == nullptr) {
            return false;
        }
        index = (index + 1U) & (kPreparedTableCapacity - 1U);
    }
    return false;
}

extern "C" void Mk64FrameInterpolation3DSGetStats(uint32_t* attempts,
                                                    uint32_t* interpolatedFrames,
                                                    uint32_t* retainedFrames,
                                                    uint32_t* matchedMatrices,
                                                    uint32_t* totalMatrices) {
    if (attempts != nullptr) *attempts = sPrepareAttempts;
    if (interpolatedFrames != nullptr) *interpolatedFrames = sPreparedFrames;
    if (retainedFrames != nullptr) *retainedFrames = sRetainedFrames;
    if (matchedMatrices != nullptr) *matchedMatrices = sLastMatchedMatrices;
    if (totalMatrices != nullptr) *totalMatrices = sLastTotalMatrices;
}

void FrameInterpolation_ApplyMatrixTransformations(Mat4*, FVector, IRotator, FVector) {
}

extern "C" {

void FrameInterpolation_ShouldInterpolateFrame(bool shouldInterpolate) {
    sAllowCurrentFrame = shouldInterpolate;
}

bool check_if_recording() {
    return sEnabled && sRecording;
}

void FrameInterpolation_StartRecord() {
    if (!sEnabled) {
        return;
    }
    sPreviousFrame = sCurrentFrame;
    sCurrentFrame ^= 1U;
    MatrixFrame& current = sStorage->frames[sCurrentFrame];
    current.count = 0;
    current.overflowed = false;
    current.cameraCut = !sAllowCurrentFrame;
    sAllowCurrentFrame = true;
    sStorage->scopes.fill({});
    sStorage->childOccurrences.fill({});
    sStorage->scopes[0].hash = 0x4D4B3634U; // "MK64"
    sScopeDepth = 1;
    sRecording = true;
}

void FrameInterpolation_StopRecord() {
    if (sEnabled && sRecording && sScopeDepth != 1U) {
        sStorage->frames[sCurrentFrame].overflowed = true;
    }
    sScopeDepth = 0;
    sRecording = false;
}

void FrameInterpolation_RecordMarker(const char* file, int line) {
    if (!sEnabled || !sRecording) return;
    ScopeState* scope = CurrentScope();
    if (scope == nullptr) {
        sStorage->frames[sCurrentFrame].overflowed = true;
        return;
    }
    uint32_t marker = scope->hash;
    marker = MixHash(marker, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file)));
    marker = MixHash(marker, static_cast<uint32_t>(line));
    marker = MixHash(marker, scope->markerOrdinal++);
    scope->markerHash = marker;
    scope->matrixOrdinal = 0;
}

void FrameInterpolation_RecordOpenChild(const void* label, uintptr_t tag) {
    if (!sEnabled || !sRecording) return;
    ScopeState* parent = CurrentScope();
    if (parent == nullptr || sScopeDepth >= sStorage->scopes.size()) {
        sStorage->frames[sCurrentFrame].overflowed = true;
        ++sScopeDepth;
        return;
    }
    uint32_t occurrence = 0;
    if (!NextChildOccurrence(parent->hash, label, tag, &occurrence)) {
        sStorage->frames[sCurrentFrame].overflowed = true;
        ++sScopeDepth;
        return;
    }
    uint32_t hash = parent->hash;
    hash = MixHash(hash, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(label)));
    hash = MixHash(hash, static_cast<uint32_t>(tag));
    hash = MixHash(hash, occurrence);
    ScopeState& child = sStorage->scopes[sScopeDepth++];
    child = {};
    child.hash = hash;
}

void FrameInterpolation_RecordCloseChild() {
    if (!sEnabled || !sRecording) return;
    if (sScopeDepth <= 1U) {
        sStorage->frames[sCurrentFrame].overflowed = true;
        return;
    }
    --sScopeDepth;
}

void FrameInterpolation_DontInterpolateCamera() {
    ++sCameraEpoch;
    if (sEnabled && sStorage != nullptr) {
        sStorage->frames[sCurrentFrame].cameraCut = true;
    }
}

int FrameInterpolation_GetCameraEpoch() {
    return static_cast<int>(sCameraEpoch);
}

void FrameInterpolation_RecordActorPosRotMatrix() {
}

void FrameInterpolation_RecordMatrixPosRotXYZ(Mat4*, Vec3f, Vec3s) {
}

void FrameInterpolation_RecordMatrixPosRotScaleXY(Mat4*, s32, s32, u16, f32) {
}

void FrameInterpolation_Record_SetTextMatrix(Mat4*, f32, f32, f32, f32) {
}

void FrameInterpolation_RecordMatrixMult(Mat4*, MtxF*, u8) {
}

void FrameInterpolation_RecordMatrixTranslate(Mat4*, Vec3f) {
}

void FrameInterpolation_RecordMatrixScale(Mat4*, f32) {
}

void FrameInterpolation_RecordMatrixRotate1Coord(Mat4*, u32, s16) {
}

void __attribute__((noinline)) FrameInterpolation_RecordMatrixMtxFToMtx(MtxF* source,
                                                                         Mtx* destination) {
    if (source != nullptr) {
        AppendMatrix(destination, *source, MatrixKind::Converted,
                     reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
    }
}

void FrameInterpolation_RecordMatrixToMtx(Mtx* destination, char* file, s32 line) {
    if (gInterpolationMatrix == nullptr) {
        return;
    }
    MtxF value = {};
    std::memcpy(&value, gInterpolationMatrix, sizeof(value));
    const uintptr_t signature = reinterpret_cast<uintptr_t>(file) ^
                                (static_cast<uintptr_t>(static_cast<uint32_t>(line)) << 1U);
    AppendMatrix(destination, value, MatrixKind::MatrixStack, signature);
}

void FrameInterpolation_RecordMatrixReplaceRotation(MtxF*) {
}

void __attribute__((noinline)) FrameInterpolation_RecordSkinMatrixMtxFToMtx(MtxF* source,
                                                                             Mtx* destination) {
    if (source != nullptr) {
        AppendMatrix(destination, *source, MatrixKind::Skin,
                     reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
    }
}

void FrameInterpolation_RecordSetTransformMatrix(Mat4*, Vec3f, Vec3f, u16, f32) {
}

void FrameInterpolation_RecordSetMatrixTransformation(Mat4*, Vec3f, Vec3su, f32) {
}

void FrameInterpolation_RecordCalculateOrientationMatrix(Mat3*, f32, f32, f32, s16) {
}

void FrameInterpolation_RecordTranslateRotate(Mat4*, Vec3f, Vec3s) {
}

} // extern "C"
