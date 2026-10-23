#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <unordered_map>
#include <vector>

namespace mk64_3ds {
// Static geometry runs: a maximal sequence of vertex-load and triangle
// commands inside an immutable display list. The first execution draws
// normally and records the run's camera-independent vertex data (object
// position, raw RSP texture coordinates, unlit shade colour) plus its
// triangle list in linear memory. Later executions evaluate only the
// material on the CPU and let PICA transform, cull, clip and depth-convert
// the recorded mesh. Command and vertex checksums, the load-state signature
// and the provenance of every slot read before it is loaded must all match,
// otherwise the run is interpreted normally. Proven read-only process storage
// can skip content hashes within its source generation. Replayed slots retain
// owned lazy vertices and reference-counted load-time transforms.

struct StaticRunVertex {
    float position[4];
    float texcoord0[2];
    float texcoord1[2];
    std::uint8_t color[4];
    float unused;
};
static_assert(sizeof(StaticRunVertex) == 40, "Matches the backend packed vertex stride");

// F3DVtx is 16 bytes on every Fast3D microcode used by MK64.
constexpr std::size_t kStaticRunSourceStride = 16;

inline std::uint32_t StaticRunHash(const void* data, std::size_t bytes,
                                   std::uint32_t hash = 2166136261U) {
    const auto* bytes8 = static_cast<const std::uint8_t*>(data);
    std::size_t i = 0;
    for (; i + 4 <= bytes; i += 4) {
        std::uint32_t word;
        std::memcpy(&word, bytes8 + i, sizeof(word));
        hash = (hash ^ word) * 16777619U;
    }
    for (; i < bytes; ++i) hash = (hash ^ bytes8[i]) * 16777619U;
    return hash;
}

struct StaticRunSlotSource {
    const void* vertex = nullptr;
    std::uint32_t loadSignature = 0;
    bool operator==(const StaticRunSlotSource& other) const {
        return vertex == other.vertex && loadSignature == other.loadSignature;
    }
};

// Vertex slots retain a reference-counted load-time transform. References are
// bounded by the RSP slot count; an unreferenced pool entry is safe to recycle.
struct StaticRunTransform {
    float matrix[4][4] = {};
    float aspect = 1.0f;
    bool fog = false;
    float fogMul = 0.0f, fogOffset = 0.0f;
    bool operator==(const StaticRunTransform& other) const {
        return aspect == other.aspect && fog == other.fog &&
            (!fog || (fogMul == other.fogMul && fogOffset == other.fogOffset)) &&
            std::memcmp(matrix, other.matrix, sizeof(matrix)) == 0;
    }
};

// Prepare the camera-dependent coefficients once, then only translate each
// mesh's centre. Keep the double fold that protects depth/decal precision.
class StaticRunMatrix {
  public:
    void Rows(const StaticRunTransform& transform, const float center[3], float out[4][4]) {
        if (!valid_ || transform.aspect != transform_.aspect ||
            std::memcmp(transform.matrix, transform_.matrix, sizeof(transform.matrix)) != 0) {
            transform_ = transform;
            for (int i = 0; i < 4; ++i) {
                rows_[0][i] = static_cast<double>(transform.matrix[i][0]) * transform.aspect;
                rows_[1][i] = transform.matrix[i][1];
                rows_[2][i] = (static_cast<double>(transform.matrix[i][2]) - transform.matrix[i][3]) * 0.5;
                rows_[3][i] = transform.matrix[i][3];
            }
            valid_ = true;
            ++preparations_;
        }
        for (int row = 0; row < 4; ++row) {
            double translation = rows_[row][3];
            for (int i = 0; i < 3; ++i) translation += static_cast<double>(center[i]) * rows_[row][i];
            for (int i = 0; i < 3; ++i) out[row][i] = static_cast<float>(rows_[row][i]);
            out[row][3] = static_cast<float>(translation);
        }
    }
    std::uint64_t Preparations() const { return preparations_; }
  private:
    StaticRunTransform transform_{};
    double rows_[4][4] = {};
    bool valid_ = false;
    std::uint64_t preparations_ = 0;
};

// Match the packed renderer's per-channel colour quantization. A material
// can take RGB from shade and alpha from a constant, or the reverse.
inline void StaticRunColor(const float input[4], bool shadeRgb, bool shadeAlpha,
                           bool useAlpha, float mask[4], float constant[4]) {
    for (int channel = 0; channel < 4; ++channel) {
        const bool varying = channel == 3 ? useAlpha && shadeAlpha : shadeRgb;
        mask[channel] = varying ? 1.0f : 0.0f;
        const float value = channel == 3 && !useAlpha ? 1.0f : input[channel];
        constant[channel] = varying || !(value > 0.0f) ? 0.0f : value >= 1.0f ? 255.0f :
            static_cast<float>(static_cast<std::uint8_t>(value * 255.0f + 0.5f));
    }
}

struct StaticRunLoad {
    const void* source;
    std::uint16_t count;
    std::uint8_t firstSlot;
    std::uint8_t pad;
    std::uint32_t firstVertex;
};

struct StaticRunInput {
    StaticRunSlotSource source;
    StaticRunVertex snapshot;
    std::uint8_t slot;
};

struct StaticRunCounters {
    std::uint64_t replays = 0, replayedTriangles = 0, captures = 0, rejected = 0;
    std::uint64_t fallbacks = 0, materializations = 0, evictions = 0;
    std::uint64_t snapshotCopies = 0, transformCopies = 0, hashedBytes = 0;
};

template <std::size_t Slots> class StaticRunCache {
    static_assert(Slots <= 256, "Slot indices are stored in one byte");

  public:
    struct Allocator {
        void* (*allocate)(std::size_t bytes) = nullptr;
        // May defer the free: the GPU can still read a block for two frames.
        void (*release)(void* pointer) = nullptr;
        bool (*publish)(const void* pointer, std::size_t bytes) = nullptr;
    };
    struct Validation {
        // A nonzero generation owns the readable source ranges. A change is
        // checked BEFORE dereferencing any old command or vertex pointer.
        std::uint32_t (*generation)() = nullptr;
        // Only certify storage that no producer can write for this generation.
        // Unknown writable resources always keep the content-hash fallback.
        bool (*immutable)(const void* pointer, std::size_t bytes) = nullptr;
    };
    // Ordinary-heap footprint is only the map node; the run's vertices,
    // indices, load ranges and required inputs share one linear block.
    struct Entry {
        const void* end = nullptr;
        void* block = nullptr;
        StaticRunVertex* vertices = nullptr;
        std::uint16_t* indices = nullptr;
        const StaticRunLoad* loads = nullptr;
        const StaticRunInput* inputs = nullptr;
        std::uint32_t commandHash = 0, vertexHash = 0, loadSignature = 0, lastFrame = 0;
        std::uint32_t bytes = 0;
        std::uint16_t vertexCount = 0, indexCount = 0;
        std::uint8_t loadCount = 0, inputCount = 0;
        bool valid = false;
        bool immutableCommands = false;
        std::uint32_t sourceGeneration = 0;
        // Stored positions are relative to this centre. The replay matrix
        // folds it back in double precision, so PICA's float24 dot products
        // only see small object offsets (depth/decal precision).
        float center[3] = {};
    };

    StaticRunCache(std::size_t budgetBytes, std::size_t maxEntries)
        : budget_(budgetBytes), maxEntries_(maxEntries) {
        entries_.reserve(maxEntries);
    }
    ~StaticRunCache() { Clear(); }
    StaticRunCache(const StaticRunCache&) = delete;
    StaticRunCache& operator=(const StaticRunCache&) = delete;

    void SetAllocator(const Allocator& allocator) { allocator_ = allocator; }
    void SetValidation(const Validation& validation) { validation_ = validation; }
    void SetEnabled(bool enabled) {
        if (!enabled && enabled_) Clear();
        enabled_ = enabled;
    }
    bool Enabled() const { return enabled_ && allocator_.allocate != nullptr; }
    StaticRunCounters& Counters() { return counters_; }
    std::size_t Bytes() const { return bytes_; }
    std::size_t EntryCount() const { return entries_.size(); }
    void BeginFrame(std::uint32_t frame) { frame_ = frame; }

    // Every vertex load, captured or not, updates the slot's provenance and
    // its camera-independent snapshot for later runs that only read it.
    void NoteLoadRange(const void* source, std::size_t count, std::size_t firstSlot) {
        if (!capturing_) return;
        if (count > 0xFFFF || firstSlot >= Slots || count > Slots - firstSlot || captureLoads_.size() >= 255) {
            AbortCapture();
            return;
        }
        captureLoads_.push_back({source, static_cast<std::uint16_t>(count),
                                 static_cast<std::uint8_t>(firstSlot), 0,
                                 static_cast<std::uint32_t>(captureVertices_.size())});
    }
    void NoteLoad(std::size_t slot, const void* source, std::uint32_t loadSignature,
                  const StaticRunVertex& vertex, const StaticRunTransform& transform = {}) {
        if (slot >= Slots) return;
        owned_[slot] = {};
        slots_[slot] = {source, loadSignature};
        virtual_[slot] = false;
        snapshot_[slot] = vertex;
        SetTransform(slot, AcquireTransform(transform));
        if (capturing_) {
            if (loadSignature != captureSignature_ || !(transform == captureTransform_) ||
                captureVertices_.size() >= 0xFFFF) {
                AbortCapture();
                return;
            }
            slotIndex_[slot] = static_cast<std::int32_t>(captureVertices_.size());
            captureVertices_.push_back(vertex);
        }
    }
    // A slot replayed from the cache holds no CPU-transformed data. Callers
    // must reload it from its source before an interpreted path reads it.
    bool IsVirtual(std::size_t slot) const { return slot < Slots && virtual_[slot]; }
    const void* VirtualSource(std::size_t slot) const { return slot < Slots ? slots_[slot].vertex : nullptr; }
    const StaticRunVertex& Snapshot(std::size_t slot) {
        Detach(slot);
        return snapshot_[slot];
    }
    const StaticRunTransform& Transform(std::size_t slot) const { return transforms_[transformIndex_[slot]].value; }
    void Materialized(std::size_t slot) {
        if (slot < Slots) virtual_[slot] = false;
        ++counters_.materializations;
    }
    // Any load whose provenance cannot be recorded (dynamic RAM, lighting,
    // texture generation) clears that slot's identity so no run can match it.
    void ForgetSlot(std::size_t slot) {
        if (slot >= Slots) return;
        slots_[slot] = {};
        owned_[slot] = {};
        virtual_[slot] = false;
        AbortCapture();
    }

    bool Capturing() const { return capturing_; }
    bool Known(const void* start) const { return entries_.find(start) != entries_.end(); }
    void BeginCapture(const void* start, std::uint32_t loadSignature,
                      const StaticRunTransform& transform = {}) {
        if (!Enabled() || entries_.size() >= maxEntries_) return;
        capturing_ = true;
        captureStart_ = start;
        captureSignature_ = loadSignature;
        captureTransform_ = transform;
        captureVertices_.clear();
        captureIndices_.clear();
        captureLoads_.clear();
        captureInputs_.clear();
        slotIndex_.fill(-1);
    }
    void AbortCapture() {
        if (capturing_) Reject(captureStart_);
        capturing_ = false;
    }
    // Record one triangle of the run before any CPU rejection; PICA culls.
    void CaptureTriangle(std::size_t a, std::size_t b, std::size_t c) {
        if (!capturing_) return;
        const std::size_t corners[3] = {a, b, c};
        for (std::size_t slot : corners) {
            if (slot >= Slots || slots_[slot].vertex == nullptr ||
                !(Transform(slot) == captureTransform_)) {
                AbortCapture();
                return;
            }
            if (slotIndex_[slot] < 0) {
                // Read before this run loads it: record the provenance it requires.
                if (captureVertices_.size() >= 0xFFFF || captureInputs_.size() >= 255) {
                    AbortCapture();
                    return;
                }
                const auto& snapshot = Snapshot(slot);
                captureInputs_.push_back({slots_[slot], snapshot, static_cast<std::uint8_t>(slot)});
                slotIndex_[slot] = static_cast<std::int32_t>(captureVertices_.size());
                captureVertices_.push_back(snapshot);
            }
        }
        if (captureIndices_.size() + 3 > 0xFFFF) {
            AbortCapture();
            return;
        }
        for (std::size_t slot : corners) captureIndices_.push_back(static_cast<std::uint16_t>(slotIndex_[slot]));
    }
    // Finish at the first command outside the run. Returns whether it stored.
    bool EndCapture(const void* end) {
        if (!capturing_) return false;
        capturing_ = false;
        if (captureIndices_.empty() || end == nullptr || end <= captureStart_) {
            Reject(captureStart_);
            return false;
        }
        const std::size_t vertexBytes = captureVertices_.size() * sizeof(StaticRunVertex);
        const std::size_t indexBytes = (captureIndices_.size() * sizeof(std::uint16_t) + 7) & ~std::size_t{7};
        const std::size_t loadBytes = captureLoads_.size() * sizeof(StaticRunLoad);
        const std::size_t inputBytes = captureInputs_.size() * sizeof(StaticRunInput);
        const std::size_t total = vertexBytes + indexBytes + loadBytes + inputBytes;
        if (total > budget_ / 4 || !MakeRoom(total)) {
            Reject(captureStart_);
            return false;
        }
        auto* block = static_cast<std::uint8_t*>(allocator_.allocate(total));
        if (block == nullptr) {
            Reject(captureStart_);
            return false;
        }
        float minimum[3], maximum[3];
        for (int axis = 0; axis < 3; ++axis) minimum[axis] = maximum[axis] = captureVertices_[0].position[axis];
        for (const auto& vertex : captureVertices_)
            for (int axis = 0; axis < 3; ++axis) {
                if (vertex.position[axis] < minimum[axis]) minimum[axis] = vertex.position[axis];
                if (vertex.position[axis] > maximum[axis]) maximum[axis] = vertex.position[axis];
            }
        float center[3];
        // N64 object coordinates are integers; an integral centre keeps the
        // relative offsets exact in float32.
        for (int axis = 0; axis < 3; ++axis)
            center[axis] = static_cast<float>(static_cast<std::int32_t>((minimum[axis] + maximum[axis]) * 0.5f));
        for (auto& vertex : captureVertices_)
            for (int axis = 0; axis < 3; ++axis) vertex.position[axis] -= center[axis];

        auto* vertices = reinterpret_cast<StaticRunVertex*>(block);
        auto* indices = reinterpret_cast<std::uint16_t*>(block + vertexBytes);
        auto* loads = reinterpret_cast<StaticRunLoad*>(block + vertexBytes + indexBytes);
        auto* inputs = reinterpret_cast<StaticRunInput*>(block + vertexBytes + indexBytes + loadBytes);
        std::memcpy(vertices, captureVertices_.data(), vertexBytes);
        std::memset(indices, 0, indexBytes);
        std::memcpy(indices, captureIndices_.data(), captureIndices_.size() * sizeof(std::uint16_t));
        if (loadBytes) std::memcpy(loads, captureLoads_.data(), loadBytes);
        if (inputBytes) std::memcpy(inputs, captureInputs_.data(), inputBytes);
        const auto generation = validation_.generation ? validation_.generation() : 0;
        if (generation && validation_.immutable)
            for (std::size_t i = 0; i < captureLoads_.size(); ++i)
                loads[i].pad = validation_.immutable(loads[i].source, loads[i].count * kStaticRunSourceStride);
        if (allocator_.publish != nullptr && !allocator_.publish(block, vertexBytes + indexBytes)) {
            allocator_.release(block);
            Reject(captureStart_);
            return false;
        }
        Entry& entry = entries_[captureStart_];
        Release(entry);
        entry = {};
        entry.end = end;
        entry.block = block;
        entry.vertices = vertices;
        entry.indices = indices;
        entry.loads = loads;
        entry.inputs = inputs;
        entry.loadCount = static_cast<std::uint8_t>(captureLoads_.size());
        entry.inputCount = static_cast<std::uint8_t>(captureInputs_.size());
        entry.loadSignature = captureSignature_;
        entry.commandHash = CommandHash(captureStart_, end);
        entry.vertexHash = VertexHash(entry);
        entry.sourceGeneration = generation;
        entry.immutableCommands = generation && validation_.immutable && validation_.immutable(captureStart_,
            static_cast<const std::uint8_t*>(end) - static_cast<const std::uint8_t*>(captureStart_));
        entry.vertexCount = static_cast<std::uint16_t>(captureVertices_.size());
        entry.indexCount = static_cast<std::uint16_t>(captureIndices_.size());
        for (int axis = 0; axis < 3; ++axis) entry.center[axis] = center[axis];
        entry.bytes = static_cast<std::uint32_t>(total);
        entry.lastFrame = frame_;
        entry.valid = true;
        bytes_ += total;
        ++counters_.captures;
        return true;
    }

    // A replayable entry for the run starting at `start`, or nullptr.
    const Entry* Find(const void* start, std::uint32_t loadSignature,
                      const StaticRunTransform& transform = {}) {
        if (!Enabled()) return nullptr;
        auto it = entries_.find(start);
        if (it == entries_.end() || !it->second.valid) return nullptr;
        Entry& entry = it->second;
        if (entry.sourceGeneration && (!validation_.generation || validation_.generation() != entry.sourceGeneration)) {
            Release(entry);
            entry = {};
            return Fallback();
        }
        if (entry.loadSignature != loadSignature) return Fallback();
        for (std::size_t i = 0; i < entry.inputCount; ++i) {
            const auto& input = entry.inputs[i];
            if (!(slots_[input.slot] == input.source) || !(Transform(input.slot) == transform) ||
                std::memcmp(&Snapshot(input.slot), &input.snapshot, sizeof(input.snapshot)) != 0)
                return Fallback();
        }
        if ((!entry.immutableCommands && CommandHash(start, entry.end) != entry.commandHash) ||
            VertexHash(entry) != entry.vertexHash) {
            // The display list or its vertices changed in place: never replay
            // stale geometry, and do not recapture mutable data every frame.
            Release(entry);
            entry = {};
            ++counters_.rejected;
            return Fallback();
        }
        entry.lastFrame = frame_;
        return &entry;
    }
    void Replayed(const Entry& entry, const StaticRunTransform& transform = {}) {
        const auto matrix = AcquireTransform(transform);
        for (std::size_t i = 0; i < entry.loadCount; ++i) {
            const StaticRunLoad& load = entry.loads[i];
            for (std::size_t k = 0; k < load.count && load.firstSlot + k < Slots; ++k) {
                const std::size_t slot = load.firstSlot + k;
                slots_[slot] = {static_cast<const std::uint8_t*>(load.source) + k * kStaticRunSourceStride,
                                entry.loadSignature};
                // The map node owns these immutable vertices. Release detaches
                // its remaining slots before freeing/retiring the GPU block.
                owned_[slot] = {&entry, static_cast<std::uint16_t>(load.firstVertex + k)};
                SetTransform(slot, matrix);
                virtual_[slot] = true;
            }
        }
        ++counters_.replays;
        counters_.replayedTriangles += entry.indexCount / 3;
    }

    void Clear() {
        for (auto& item : entries_) Release(item.second);
        entries_.clear();
        bytes_ = 0;
        capturing_ = false;
        for (std::size_t slot = 0; slot < Slots; ++slot) {
            slots_[slot] = {};
            // Clear can follow a live settings change. The old CPU slots still
            // need their load-time values until replaced or materialized.
        }
    }

  private:
    struct OwnedSlot { const Entry* entry = nullptr; std::uint16_t vertex = 0; };
    struct TransformOwner { StaticRunTransform value{}; std::uint16_t references = 0; bool valid = false; };
    std::uint16_t AcquireTransform(const StaticRunTransform& value) {
        if (transforms_[lastTransform_].valid && transforms_[lastTransform_].value == value)
            return lastTransform_;
        for (std::size_t i = 0; i < transforms_.size(); ++i) {
            if (transforms_[i].references != 0) continue;
            transforms_[i].value = value;
            transforms_[i].valid = true;
            ++counters_.transformCopies;
            return lastTransform_ = static_cast<std::uint16_t>(i);
        }
        // Slots+1 entries for at most Slots referenced matrices.
        std::abort();
    }
    void SetTransform(std::size_t slot, std::uint16_t index) {
        if (hasTransform_[slot]) --transforms_[transformIndex_[slot]].references;
        transformIndex_[slot] = index;
        hasTransform_[slot] = true;
        ++transforms_[index].references;
    }
    void Detach(std::size_t slot) {
        const auto owner = owned_[slot];
        if (owner.entry == nullptr) return;
        snapshot_[slot] = owner.entry->vertices[owner.vertex];
        for (int axis = 0; axis < 3; ++axis) snapshot_[slot].position[axis] += owner.entry->center[axis];
        owned_[slot] = {};
        ++counters_.snapshotCopies;
    }
    const Entry* Fallback() {
        ++counters_.fallbacks;
        return nullptr;
    }
    void Reject(const void* start) {
        // Remember uncacheable runs so they are not re-captured every frame.
        if (entries_.size() >= maxEntries_ && entries_.find(start) == entries_.end()) return;
        Entry& entry = entries_[start];
        Release(entry);
        entry = {};
        ++counters_.rejected;
    }
    std::uint32_t CommandHash(const void* start, const void* end) {
        const auto bytes = static_cast<const std::uint8_t*>(end) - static_cast<const std::uint8_t*>(start);
        counters_.hashedBytes += bytes;
        return StaticRunHash(start, bytes);
    }
    std::uint32_t VertexHash(const Entry& entry) {
        std::uint32_t hash = 2166136261U;
        for (std::size_t i = 0; i < entry.loadCount; ++i) {
            if (entry.loads[i].pad) continue;
            counters_.hashedBytes += entry.loads[i].count * kStaticRunSourceStride;
            hash = StaticRunHash(entry.loads[i].source, entry.loads[i].count * kStaticRunSourceStride, hash);
        }
        return hash;
    }
    void Release(Entry& entry) {
        // Detach only on retirement, never on the common replay hit. A slot
        // retains its value even if the source resource has already mutated.
        if (entry.block)
            for (std::size_t slot = 0; slot < Slots; ++slot)
                if (owned_[slot].entry == &entry) Detach(slot);
        if (entry.block) allocator_.release(entry.block);
        if (entry.valid) bytes_ -= entry.bytes < bytes_ ? entry.bytes : bytes_;
        entry.block = nullptr;
        entry.vertices = nullptr;
        entry.indices = nullptr;
        entry.valid = false;
    }
    bool MakeRoom(std::size_t bytes) {
        while (bytes_ + bytes > budget_) {
            auto oldest = entries_.end();
            for (auto it = entries_.begin(); it != entries_.end(); ++it) {
                if (!it->second.valid || it->second.lastFrame == frame_) continue;
                if (oldest == entries_.end() || it->second.lastFrame < oldest->second.lastFrame) oldest = it;
            }
            if (oldest == entries_.end()) return false;
            Release(oldest->second);
            entries_.erase(oldest);
            ++counters_.evictions;
        }
        return true;
    }

    Allocator allocator_;
    Validation validation_;
    std::size_t budget_ = 0, maxEntries_ = 0, bytes_ = 0;
    bool enabled_ = true, capturing_ = false;
    std::uint32_t frame_ = 0, captureSignature_ = 0;
    std::unordered_map<const void*, Entry> entries_;
    StaticRunCounters counters_;
    const void* captureStart_ = nullptr;
    std::vector<StaticRunVertex> captureVertices_;
    std::vector<std::uint16_t> captureIndices_;
    std::vector<StaticRunLoad> captureLoads_;
    std::vector<StaticRunInput> captureInputs_;
    StaticRunSlotSource slots_[Slots] = {};
    bool virtual_[Slots] = {};
    StaticRunVertex snapshot_[Slots] = {};
    OwnedSlot owned_[Slots] = {};
    std::array<TransformOwner, Slots + 1> transforms_{};
    std::uint16_t transformIndex_[Slots] = {};
    bool hasTransform_[Slots] = {};
    std::uint16_t lastTransform_ = 0;
    StaticRunTransform captureTransform_{};
    std::array<std::int32_t, Slots> slotIndex_{};
};
} // namespace mk64_3ds
