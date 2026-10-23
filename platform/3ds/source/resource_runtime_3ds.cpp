#include "resource_name_cache_3ds.hpp"
extern "C" bool Mk64NativeKartReclaimCache() __attribute__((weak));
#include "resource_runtime_3ds.h"
#include "heap_policy_3ds.hpp"

#include "o2r_archive_reader.hpp"
#include "texture_retention_3ds.hpp"

#define MK64_3DS_LIBULTRA_ONLY
#include <libultraship/libultra/gbi.h>
#include <ship/utils/StrHash64.h>

#include "audio/internal.h"
#include "waypoints.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#define MK64_OPTIONAL_SYMBOL __attribute__((weak_import))
#else
#define MK64_OPTIONAL_SYMBOL __attribute__((weak))
#endif
extern "C" bool Mk64Improved3DSReclaim(void) MK64_OPTIONAL_SYMBOL;
extern "C" const uint8_t* Mk64NativeKartPaletteData(const char*) MK64_OPTIONAL_SYMBOL;
extern "C" void Mk64Perf3DSPreparedPaletteHit(void) MK64_OPTIONAL_SYMBOL;
extern "C" uint64_t Mk64Perf3DSResourceStart(void) MK64_OPTIONAL_SYMBOL;
extern "C" void Mk64Perf3DSResourceRead(uint64_t, size_t) MK64_OPTIONAL_SYMBOL;
extern "C" void Mk64Diagnostics3DSSetResource(const char*, size_t) MK64_OPTIONAL_SYMBOL;
extern "C" void Mk64Diagnostics3DSSetArchiveEntryCount(size_t) MK64_OPTIONAL_SYMBOL;
extern "C" void Mk64Diagnostics3DSFailure(const char*, const char*) MK64_OPTIONAL_SYMBOL;
extern "C" void Mk64Diagnostics3DSCheckpoint(const char*) MK64_OPTIONAL_SYMBOL;
extern "C" void AudioDma_Register(const void* base, size_t size) MK64_OPTIONAL_SYMBOL;
extern "C" void AudioDma_Clear(void) MK64_OPTIONAL_SYMBOL;
extern "C" void Mk64Graphics3DSEvictSourceTexture(const void*) MK64_OPTIONAL_SYMBOL;
extern "C" void* Mk64ResourceHeapAllocate(size_t) MK64_OPTIONAL_SYMBOL;
extern "C" void Mk64ResourceHeapFree(void*) MK64_OPTIONAL_SYMBOL;
extern "C" void Mk64GameAudio3DSServiceLoading(void) MK64_OPTIONAL_SYMBOL;

namespace {

constexpr size_t kOtrHeaderSize = 64;
constexpr std::string_view kOtrSignature = "__OTR__";

constexpr uint32_t MakeType(char a, char b, char c, char d) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d);
}

constexpr uint32_t kTypeAudioSample = MakeType('A', 'U', 'F', 'C');
constexpr uint32_t kTypeAudioBank = MakeType('B', 'A', 'N', 'K');
constexpr uint32_t kTypeCpu = MakeType('D', 'B', 'H', 'V');
constexpr uint32_t kTypeGenericArray = MakeType('G', 'A', 'R', 'R');
constexpr uint32_t kTypeDisplayList = MakeType('O', 'D', 'L', 'T');
constexpr uint32_t kTypeMatrix = MakeType('O', 'M', 'T', 'X');
constexpr uint32_t kTypeTexture = MakeType('O', 'T', 'E', 'X');
constexpr uint32_t kTypeVertex = MakeType('O', 'V', 'T', 'X');
constexpr uint32_t kTypePaths = MakeType('P', 'A', 'T', 'S');
constexpr uint32_t kTypeTrackSection = MakeType('S', 'C', 'T', 'N');
constexpr uint32_t kTypeSpawnData = MakeType('S', 'D', 'A', 'T');
constexpr uint32_t kTypeSequence = MakeType('S', 'E', 'Q', 'C');
constexpr uint32_t kTypeUnknownSpawnData = MakeType('U', 'S', 'D', 'T');
constexpr uint32_t kTypeLight = 0x46669697;
constexpr uint8_t kUcodeFast3DEX = 2;

struct CpuBehaviour3DS {
    int16_t pathPointStart;
    int16_t pathPointEnd;
    int32_t type;
};

struct TrackSection3DS {
    union {
        uint64_t crc;
        void* model;
    };
    uint8_t surfaceType;
    uint8_t sectionId;
    uint16_t clip;
    uint16_t layer;
    float location[3];
};

struct ActorSpawn3DS {
    int16_t pos[3];
    int16_t id;
};

struct UnknownActorSpawn3DS {
    int16_t pos[3];
    int16_t id;
    int16_t originalY;
};

struct AmbientData3DS {
    uint8_t color[3];
    int8_t pad1;
    uint8_t colorCopy[3];
    int8_t pad2;
};

union LightData3DS {
    uint8_t bytes[16];
    long long alignment[2];
};

struct LightEntry3DS {
    AmbientData3DS ambient;
    LightData3DS light;
};

static_assert(sizeof(CpuBehaviour3DS) == 8);
static_assert(sizeof(TrackPathPoint) == 8);
static_assert(sizeof(ActorSpawn3DS) == 8);
static_assert(sizeof(UnknownActorSpawn3DS) == 10);
static_assert(sizeof(LightEntry3DS) == 24);

class Reader final {
  public:
    Reader(const std::vector<uint8_t>& bytes, size_t offset) : mBytes(bytes), mOffset(offset) {
    }

    bool CanRead(size_t count) const {
        return count <= mBytes.size() - std::min(mOffset, mBytes.size());
    }

    size_t Offset() const {
        return mOffset;
    }

    bool Align(size_t alignment) {
        const size_t aligned = (mOffset + alignment - 1) & ~(alignment - 1);
        if (aligned > mBytes.size()) {
            return false;
        }
        mOffset = aligned;
        return true;
    }

    bool ReadBytes(void* destination, size_t count) {
        if (destination == nullptr || !CanRead(count)) {
            return false;
        }
        std::memcpy(destination, mBytes.data() + mOffset, count);
        mOffset += count;
        return true;
    }

    bool Skip(size_t count) {
        if (!CanRead(count)) {
            return false;
        }
        mOffset += count;
        return true;
    }

    bool ReadU8(uint8_t* value) {
        return ReadBytes(value, sizeof(*value));
    }

    bool ReadS16(int16_t* value) {
        uint16_t raw = 0;
        if (!ReadU16(&raw)) {
            return false;
        }
        *value = static_cast<int16_t>(raw);
        return true;
    }

    bool ReadU16(uint16_t* value) {
        std::array<uint8_t, 2> raw = {};
        if (!ReadBytes(raw.data(), raw.size())) {
            return false;
        }
        *value = static_cast<uint16_t>(raw[0]) | (static_cast<uint16_t>(raw[1]) << 8);
        return true;
    }

    bool ReadS32(int32_t* value) {
        uint32_t raw = 0;
        if (!ReadU32(&raw)) {
            return false;
        }
        *value = static_cast<int32_t>(raw);
        return true;
    }

    bool ReadU32(uint32_t* value) {
        std::array<uint8_t, 4> raw = {};
        if (!ReadBytes(raw.data(), raw.size())) {
            return false;
        }
        *value = static_cast<uint32_t>(raw[0]) | (static_cast<uint32_t>(raw[1]) << 8) |
                 (static_cast<uint32_t>(raw[2]) << 16) | (static_cast<uint32_t>(raw[3]) << 24);
        return true;
    }

    bool ReadU64(uint64_t* value) {
        uint32_t low = 0;
        uint32_t high = 0;
        if (!ReadU32(&low) || !ReadU32(&high)) {
            return false;
        }
        *value = static_cast<uint64_t>(low) | (static_cast<uint64_t>(high) << 32);
        return true;
    }

    bool ReadFloat(float* value) {
        uint32_t bits = 0;
        if (!ReadU32(&bits)) {
            return false;
        }
        std::memcpy(value, &bits, sizeof(bits));
        return true;
    }

    bool ReadStringView(std::string_view* value) {
        int32_t count = 0;
        if (value == nullptr || !ReadS32(&count) || count < 0 || !CanRead(static_cast<size_t>(count))) {
            return false;
        }
        *value = std::string_view(reinterpret_cast<const char*>(mBytes.data() + mOffset),
                                  static_cast<size_t>(count));
        mOffset += static_cast<size_t>(count);
        return true;
    }

  private:
    const std::vector<uint8_t>& mBytes;
    size_t mOffset;
};

struct CrcEntry;

void* AllocateResourceBytes(size_t byteCount) {
    return Mk64ResourceHeapAllocate != nullptr ? Mk64ResourceHeapAllocate(byteCount)
                                               : std::malloc(byteCount);
}

void FreeResourceBytes(void* bytes) {
    if (bytes == nullptr) {
        return;
    }
    if (Mk64ResourceHeapFree != nullptr) {
        Mk64ResourceHeapFree(bytes);
    } else {
        std::free(bytes);
    }
}

struct LoadedResource {
    ~LoadedResource() {
        for (size_t i = 0; i < inlineAllocationCount; ++i) {
            FreeResourceBytes(inlineAllocations[i]);
        }
        if (extraAllocations != nullptr) {
            for (void* allocation : *extraAllocations) {
                FreeResourceBytes(allocation);
            }
        }
    }

    template <typename T> T* Allocate(size_t count = 1) {
        if (count == 0 || count > SIZE_MAX / sizeof(T)) {
            return nullptr;
        }
        const size_t byteCount = sizeof(T) * count;
        // Parsed O2R payloads are CPU-side immutable source data. Fast3D
        // decodes/copies them into Citro3D-owned linear textures and packed
        // vertex buffers before submission, while the audio HLE mixer reads
        // samples on the CPU. Keeping these thousands of long-lived objects
        // out of the small linear heap leaves that heap for resources which
        // the GPU and NDSP actually require.
        void* bytes = AllocateResourceBytes(byteCount);
        if (bytes == nullptr) {
            throw std::bad_alloc();
        }
        std::memset(bytes, 0, byteCount);
        T* result = reinterpret_cast<T*>(bytes);
        try {
            if (inlineAllocationCount < inlineAllocations.size()) {
                inlineAllocations[inlineAllocationCount++] = bytes;
            } else {
                if (extraAllocations == nullptr) {
                    extraAllocations = std::make_unique<std::vector<void*>>();
                    extraAllocations->reserve(8);
                }
                extraAllocations->emplace_back(bytes);
            }
        } catch (...) {
            FreeResourceBytes(bytes);
            throw;
        }
        allocatedBytes += byteCount;
        return result;
    }

    uint32_t type = 0;
    uint32_t version = 0;
    void* pointer = nullptr;
    size_t pointerSize = 0;
    size_t allocatedBytes = 0;
    uint16_t textureWidth = 0;
    uint16_t textureHeight = 0;
    uint32_t textureType = 0;
    size_t archiveIndex = SIZE_MAX;
    CrcEntry* crcEntry = nullptr;
    uint32_t textureOwners = 0;
    bool evictWhenUnused = false;
    bool rawBorrowed = false;
    // An explicit eviction arrived while owned: free on the last release.
    bool evictionRequested = false;
    // Unowned but kept decoded in the bounded retention queue below.
    bool retained = false;
    uint32_t retainStamp = 0;
    // Nearly every graphics resource owns one allocation. Keep that pointer
    // inline; only the small, permanently pinned sound graph needs overflow.
    std::array<void*, 1> inlineAllocations = {};
    uint8_t inlineAllocationCount = 0;
    std::unique_ptr<std::vector<void*>> extraAllocations;
};

std::unique_ptr<mk64_3ds::O2rArchiveReader> sArchive;

struct CrcEntry {
    uint64_t crc = 0;
    LoadedResource* loaded = nullptr;
};

std::vector<std::unique_ptr<LoadedResource>> sLoadedResources;
std::vector<std::pair<uintptr_t,size_t>> sDisplayListAddresses;
std::vector<CrcEntry> sCrcEntries;
std::vector<uint32_t> sCrcSlots;
mk64_3ds::ResourceNameCache<> sResourceNames;
uint64_t sResourceNameHits=0,sResourceNameMisses=0;
size_t sLoadedResourceCount = 0;
std::array<size_t, UINT8_MAX + 1U> sBanksById = {};
std::array<size_t, UINT8_MAX + 1U> sSequencesById = {};
std::array<CtlEntry*, UINT8_MAX + 1U> sPinnedBanks = {};
std::array<AudioSequenceData*, UINT8_MAX + 1U> sPinnedSequences = {};
uint32_t sPinnedSequenceCount = 0;
struct ResourceLoadEvent {
    uint64_t started=0;
    uint32_t index=0,serializedBytes=0,physicalBytes=0;
};
std::array<ResourceLoadEvent,64> sLoadEvents{};
size_t sLoadEventNext=0,sLoadEventCount=0;
std::vector<uint8_t> sReadScratch;
bool sReadScratchInUse = false;
uint32_t sRuntimeGeneration = 0;

// Recently released decoded textures stay resident within this budget; see
// texture_retention_3ds.hpp. Heap headroom during a race is under 1 MiB on
// Old 3DS, so keep it small. Explicit evictions and memory pressure free it.
mk64_3ds::TextureRetention3DS<512> sRetainedTextures(256U * 1024U);
bool ReleaseRetainedTextures();
enum class ReclaimedCache : uint8_t { None, Improved, Retained, Native, Archive, Streaming };
ReclaimedCache sLastReclaimedCache = ReclaimedCache::None;
bool sReclaimingCaches = false;
ReclaimedCache ReclaimOptionalCaches(uint32_t allowed);

constexpr size_t kReadScratchLimit = 256U * 1024U;

LoadedResource* LoadByPath(std::string_view path);

std::string_view NormalizePath(const char* name) {
    if (name == nullptr) {
        return {};
    }
    std::string_view path(name);
    if (path.size() >= kOtrSignature.size() && path.substr(0, kOtrSignature.size()) == kOtrSignature) {
        path.remove_prefix(kOtrSignature.size());
    }
    return path;
}

uint64_t PathCrc(std::string_view path) {
    // crc64() complements its result while the resource system's historical
    // CRC64(string) helper does not. Complement the length-aware form so a
    // string_view can be hashed without allocating a temporary C string.
    return ~crc64(path.data(), static_cast<uint32_t>(path.size()));
}

bool CopyBlock(Reader& reader, LoadedResource& resource, size_t bytes) {
    uint8_t* data = resource.Allocate<uint8_t>(bytes);
    if (data == nullptr || !reader.ReadBytes(data, bytes)) {
        return false;
    }
    resource.pointer = data;
    resource.pointerSize = bytes;
    return true;
}

bool ParseTexture(Reader& reader, LoadedResource& resource) {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t dataSize = 0;
    if (!reader.ReadU32(&resource.textureType) || !reader.ReadU32(&width) || !reader.ReadU32(&height) ||
        !reader.ReadU32(&dataSize) || width > UINT16_MAX || height > UINT16_MAX) {
        return false;
    }
    resource.textureWidth = static_cast<uint16_t>(width);
    resource.textureHeight = static_cast<uint16_t>(height);
    return CopyBlock(reader, resource, dataSize);
}

bool ParseVertex(Reader& reader, LoadedResource& resource) {
    uint32_t count = 0;
    if (!reader.ReadU32(&count) || count > SIZE_MAX / sizeof(Vtx)) {
        return false;
    }
    return CopyBlock(reader, resource, static_cast<size_t>(count) * sizeof(Vtx));
}

bool ParseDisplayList(Reader& reader, LoadedResource& resource, const std::vector<uint8_t>& bytes) {
    uint8_t ucode = 0;
    if (!reader.ReadU8(&ucode) || ucode != kUcodeFast3DEX || !reader.Align(8)) {
        return false;
    }
    const size_t commandBytes = bytes.size() - reader.Offset();
    constexpr size_t kSerializedCommandSize = sizeof(uint32_t) * 2;
    if (commandBytes == 0 || commandBytes % kSerializedCommandSize != 0) {
        return false;
    }
    const size_t commandCount = commandBytes / kSerializedCommandSize;
    Gfx* commands = resource.Allocate<Gfx>(commandCount);
    if (commands == nullptr) {
        return false;
    }
    for (size_t i = 0; i < commandCount; ++i) {
        uint32_t word0 = 0;
        uint32_t word1 = 0;
        if (!reader.ReadU32(&word0) || !reader.ReadU32(&word1)) {
            return false;
        }
        commands[i].words.w0 = word0;
        commands[i].words.w1 = word1;
    }
    resource.pointer = commands;
    resource.pointerSize = commandCount * sizeof(Gfx);
    return true;
}

bool ParseMatrix(Reader& reader, LoadedResource& resource) {
    return CopyBlock(reader, resource, sizeof(Mtx));
}

bool ParseLight(Reader& reader, LoadedResource& resource) {
    return CopyBlock(reader, resource, sizeof(LightEntry3DS));
}

bool ParseCpu(Reader& reader, LoadedResource& resource) {
    uint32_t count = 0;
    if (!reader.ReadU32(&count) || count > SIZE_MAX / sizeof(CpuBehaviour3DS)) {
        return false;
    }
    return CopyBlock(reader, resource, static_cast<size_t>(count) * sizeof(CpuBehaviour3DS));
}

bool ParsePaths(Reader& reader, LoadedResource& resource) {
    uint32_t count = 0;
    if (!reader.ReadU32(&count) || count > SIZE_MAX / sizeof(TrackPathPoint)) {
        return false;
    }
    return CopyBlock(reader, resource, static_cast<size_t>(count) * sizeof(TrackPathPoint));
}

bool ParseTrackSections(Reader& reader, LoadedResource& resource) {
    uint32_t count = 0;
    if (!reader.ReadU32(&count)) {
        return false;
    }
    TrackSection3DS* sections = resource.Allocate<TrackSection3DS>(count);
    if (sections == nullptr) {
        return false;
    }
    for (uint32_t i = 0; i < count; ++i) {
        if (!reader.ReadU64(&sections[i].crc) || !reader.ReadU8(&sections[i].surfaceType) ||
            !reader.ReadU8(&sections[i].sectionId) || !reader.ReadU16(&sections[i].clip)) {
            return false;
        }
    }
    resource.pointer = sections;
    resource.pointerSize = static_cast<size_t>(count) * sizeof(TrackSection3DS);
    return true;
}

bool ParseSpawnData(Reader& reader, LoadedResource& resource) {
    uint32_t count = 0;
    if (!reader.ReadU32(&count) || count > SIZE_MAX / sizeof(ActorSpawn3DS)) {
        return false;
    }
    return CopyBlock(reader, resource, static_cast<size_t>(count) * sizeof(ActorSpawn3DS));
}

bool ParseUnknownSpawnData(Reader& reader, LoadedResource& resource) {
    uint32_t count = 0;
    if (!reader.ReadU32(&count) || count > SIZE_MAX / sizeof(UnknownActorSpawn3DS)) {
        return false;
    }
    return CopyBlock(reader, resource, static_cast<size_t>(count) * sizeof(UnknownActorSpawn3DS));
}

size_t GenericElementSize(uint32_t type) {
    constexpr std::array<size_t, 16> sizes = { 1, 1, 2, 2, 4, 4, 8, 4, 8, 8, 12, 6, 12, 12, 16, 8 };
    return type < sizes.size() ? sizes[type] : 0;
}

bool ParseGenericArray(Reader& reader, LoadedResource& resource) {
    uint32_t elementType = 0;
    uint32_t count = 0;
    if (!reader.ReadU32(&elementType) || !reader.ReadU32(&count)) {
        return false;
    }
    const size_t elementSize = GenericElementSize(elementType);
    if (elementSize == 0 || count > SIZE_MAX / elementSize) {
        return false;
    }
    return CopyBlock(reader, resource, static_cast<size_t>(count) * elementSize);
}

bool ParseAudioSample(Reader& reader, LoadedResource& resource) {
    AudioBankSample* sample = resource.Allocate<AudioBankSample>();
    AdpcmLoop* loop = resource.Allocate<AdpcmLoop>();
    AdpcmBook* book = resource.Allocate<AdpcmBook>();
    uint32_t loopStart = 0;
    uint32_t loopEnd = 0;
    uint32_t loopCount = 0;
    uint32_t loopPad = 0;
    if (sample == nullptr || loop == nullptr || book == nullptr || !reader.ReadU32(&loopStart) ||
        !reader.ReadU32(&loopEnd) || !reader.ReadU32(&loopCount) || !reader.ReadU32(&loopPad)) {
        return false;
    }
    loop->start = loopStart;
    loop->end = loopEnd;
    loop->count = loopCount;
    loop->pad = loopPad;

    uint32_t stateSize = 0;
    if (!reader.ReadU32(&stateSize)) {
        return false;
    }
    if (stateSize > 0) {
        loop->state = resource.Allocate<int16_t>(stateSize);
        if (loop->state == nullptr || !reader.ReadBytes(loop->state, static_cast<size_t>(stateSize) * sizeof(int16_t))) {
            return false;
        }
    }

    int32_t order = 0;
    int32_t predictors = 0;
    if (!reader.ReadS32(&order) || !reader.ReadS32(&predictors)) {
        return false;
    }
    book->order = order;
    book->npredictors = predictors;
    uint32_t tableSize = 0;
    if (!reader.ReadU32(&tableSize)) {
        return false;
    }
    book->book = resource.Allocate<int16_t>(tableSize);
    if ((tableSize > 0 && book->book == nullptr) ||
        !reader.ReadBytes(book->book, static_cast<size_t>(tableSize) * sizeof(int16_t))) {
        return false;
    }

    int32_t sampleSize = 0;
    if (!reader.ReadS32(&sampleSize) || sampleSize < 0) {
        return false;
    }
    sample->sampleAddr = resource.Allocate<uint8_t>(static_cast<size_t>(sampleSize) + 16);
    if (sample->sampleAddr == nullptr || !reader.ReadBytes(sample->sampleAddr, static_cast<size_t>(sampleSize))) {
        return false;
    }
    if (AudioDma_Register != nullptr) {
        AudioDma_Register(sample->sampleAddr, static_cast<size_t>(sampleSize));
    }
    sample->unused = 0;
    sample->loaded = 1;
    sample->loop = loop;
    sample->book = book;
    sample->sampleSize = static_cast<uint32_t>(sampleSize);
    resource.pointer = sample;
    resource.pointerSize = sizeof(*sample);
    return true;
}

bool ParseEnvelope(Reader& reader, LoadedResource& resource, AdsrEnvelope** envelope) {
    uint32_t count = 0;
    if (envelope == nullptr || !reader.ReadU32(&count)) {
        return false;
    }
    if (count == 0) {
        *envelope = nullptr;
        return true;
    }
    *envelope = resource.Allocate<AdsrEnvelope>(count);
    if (*envelope == nullptr) {
        return false;
    }
    for (uint32_t i = 0; i < count; ++i) {
        int16_t delay = 0;
        int16_t argument = 0;
        if (!reader.ReadS16(&delay) || !reader.ReadS16(&argument)) {
            return false;
        }
        (*envelope)[i].delay = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(delay)));
        (*envelope)[i].arg = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(argument)));
    }
    return true;
}

bool ParseBankSound(Reader& reader, LoadedResource& resource, AudioBankSound* sound) {
    std::string_view sampleName;
    if (sound == nullptr || !reader.ReadStringView(&sampleName)) {
        return false;
    }
    LoadedResource* sample = LoadByPath(sampleName);
    if (sample == nullptr || sample->type != kTypeAudioSample || !reader.ReadFloat(&sound->tuning)) {
        return false;
    }
    sound->sample = static_cast<AudioBankSample*>(sample->pointer);
    return true;
}

bool ParseAudioBank(Reader& reader, LoadedResource& resource) {
    uint32_t bankId = 0;
    uint32_t instrumentCount = 0;
    if (!reader.ReadU32(&bankId) || bankId > UINT8_MAX || !reader.ReadU32(&instrumentCount) ||
        instrumentCount > UINT8_MAX) {
        return false;
    }

    CtlEntry* bank = resource.Allocate<CtlEntry>();
    Instrument** instruments = resource.Allocate<Instrument*>(instrumentCount);
    if (bank == nullptr || (instrumentCount > 0 && instruments == nullptr)) {
        return false;
    }
    for (uint32_t i = 0; i < instrumentCount; ++i) {
        uint8_t valid = 0;
        if (!reader.ReadU8(&valid)) {
            return false;
        }
        if (valid == 0) {
            continue;
        }
        Instrument* instrument = resource.Allocate<Instrument>();
        if (instrument == nullptr || !reader.ReadU8(&instrument->releaseRate) ||
            !reader.ReadU8(&instrument->normalRangeLo) || !reader.ReadU8(&instrument->normalRangeHi) ||
            !ParseEnvelope(reader, resource, &instrument->envelope)) {
            return false;
        }
        instrument->loaded = 1;
        uint32_t soundFlags = 0;
        if (!reader.ReadU32(&soundFlags) ||
            ((soundFlags & 1U) != 0 && !ParseBankSound(reader, resource, &instrument->lowNotesSound)) ||
            ((soundFlags & 2U) != 0 && !ParseBankSound(reader, resource, &instrument->normalNotesSound)) ||
            ((soundFlags & 4U) != 0 && !ParseBankSound(reader, resource, &instrument->highNotesSound))) {
            return false;
        }
        instruments[i] = instrument;
    }

    uint32_t drumCount = 0;
    if (!reader.ReadU32(&drumCount) || drumCount > UINT8_MAX) {
        return false;
    }
    Drum** drums = resource.Allocate<Drum*>(drumCount);
    if (drumCount > 0 && drums == nullptr) {
        return false;
    }
    for (uint32_t i = 0; i < drumCount; ++i) {
        Drum* drum = resource.Allocate<Drum>();
        if (drum == nullptr || !reader.ReadU8(&drum->releaseRate) || !reader.ReadU8(&drum->pan) ||
            !ParseEnvelope(reader, resource, &drum->envelope) || !ParseBankSound(reader, resource, &drum->sound)) {
            return false;
        }
        drum->loaded = 1;
        drums[i] = drum;
    }

    bank->bankId = static_cast<uint8_t>(bankId);
    bank->numInstruments = static_cast<uint8_t>(instrumentCount);
    bank->numDrums = static_cast<uint8_t>(drumCount);
    bank->instruments = instruments;
    bank->drums = drums;
    resource.pointer = bank;
    resource.pointerSize = sizeof(*bank);
    return true;
}

bool ParseSequence(Reader& reader, LoadedResource& resource) {
    uint32_t id = 0;
    uint32_t bankCount = 0;
    if (!reader.ReadU32(&id) || id > UINT8_MAX || !reader.ReadU32(&bankCount)) {
        return false;
    }
    uint8_t* banks = resource.Allocate<uint8_t>(bankCount);
    if (bankCount > 0 && banks == nullptr) {
        return false;
    }
    for (uint32_t i = 0; i < bankCount; ++i) {
        std::string_view bankName;
        if (!reader.ReadStringView(&bankName)) {
            return false;
        }
        LoadedResource* bankResource = LoadByPath(bankName);
        if (bankResource == nullptr || bankResource->type != kTypeAudioBank) {
            return false;
        }
        banks[i] = static_cast<CtlEntry*>(bankResource->pointer)->bankId;
    }

    uint32_t dataSize = 0;
    if (!reader.ReadU32(&dataSize)) {
        return false;
    }
    uint8_t* data = resource.Allocate<uint8_t>(dataSize);
    if ((dataSize > 0 && data == nullptr) || !reader.ReadBytes(data, dataSize)) {
        return false;
    }

    AudioSequenceData* sequence = resource.Allocate<AudioSequenceData>();
    if (sequence == nullptr) {
        return false;
    }
    sequence->bankCount = bankCount;
    sequence->banks = banks;
    sequence->data = data;
    sequence->id = static_cast<uint8_t>(id);
    resource.pointer = sequence;
    resource.pointerSize = sizeof(*sequence);
    return true;
}

bool ParseResource(const std::vector<uint8_t>& bytes, LoadedResource& resource) {
    if (bytes.size() < kOtrHeaderSize || bytes[0] != 0) {
        return false;
    }
    Reader header(bytes, 4);
    if (!header.ReadU32(&resource.type) || !header.ReadU32(&resource.version) || resource.version != 0) {
        return false;
    }
    Reader reader(bytes, kOtrHeaderSize);
    switch (resource.type) {
        case kTypeTexture:
            return ParseTexture(reader, resource);
        case kTypeVertex:
            return ParseVertex(reader, resource);
        case kTypeDisplayList:
            return ParseDisplayList(reader, resource, bytes);
        case kTypeMatrix:
            return ParseMatrix(reader, resource);
        case kTypeLight:
            return ParseLight(reader, resource);
        case kTypeCpu:
            return ParseCpu(reader, resource);
        case kTypePaths:
            return ParsePaths(reader, resource);
        case kTypeTrackSection:
            return ParseTrackSections(reader, resource);
        case kTypeSpawnData:
            return ParseSpawnData(reader, resource);
        case kTypeUnknownSpawnData:
            return ParseUnknownSpawnData(reader, resource);
        case kTypeGenericArray:
            return ParseGenericArray(reader, resource);
        case kTypeAudioSample:
            return ParseAudioSample(reader, resource);
        case kTypeAudioBank:
            return ParseAudioBank(reader, resource);
        case kTypeSequence:
            return ParseSequence(reader, resource);
        default:
            return false;
    }
}

size_t CrcSlot(uint64_t crc) {
    const uint32_t folded = static_cast<uint32_t>(crc) ^ static_cast<uint32_t>(crc >> 32U);
    return static_cast<size_t>(folded) & (sCrcSlots.size() - 1U);
}

size_t CrcEntryArchiveIndex(const CrcEntry* entry) {
    return entry == nullptr ? SIZE_MAX : static_cast<size_t>(entry - sCrcEntries.data());
}

CrcEntry* FindCrcEntry(uint64_t crc) {
    if (sCrcSlots.empty()) {
        return nullptr;
    }
    size_t slot = CrcSlot(crc);
    for (size_t probe = 0; probe < sCrcSlots.size(); ++probe) {
        const uint32_t encodedIndex = sCrcSlots[slot];
        if (encodedIndex == 0) {
            return nullptr;
        }
        CrcEntry& entry = sCrcEntries[encodedIndex - 1U];
        if (entry.crc == crc) {
            return &entry;
        }
        slot = (slot + 1U) & (sCrcSlots.size() - 1U);
    }
    return nullptr;
}

CrcEntry* FindCrcEntry(std::string_view path,uint64_t* computedCrc=nullptr) {
    if (sArchive == nullptr || path.empty() || sCrcSlots.empty()) {
        return nullptr;
    }
    const uint32_t cached=sResourceNames.Find(path,[](size_t index)->std::string_view {
        return index<sCrcEntries.size() && index<sArchive->Entries().size()
            ? std::string_view(sArchive->Entries()[index]) : std::string_view{};
    });
    if(cached){
        ++sResourceNameHits;
        if(computedCrc)*computedCrc=sCrcEntries[cached-1].crc;
        return &sCrcEntries[cached-1];
    }
    ++sResourceNameMisses;
    const uint64_t crc = PathCrc(path);
    if(computedCrc)*computedCrc=crc;
    size_t slot = CrcSlot(crc);
    for (size_t probe = 0; probe < sCrcSlots.size(); ++probe) {
        const uint32_t encodedIndex = sCrcSlots[slot];
        if (encodedIndex == 0) {
            return nullptr;
        }
        CrcEntry& entry = sCrcEntries[encodedIndex - 1U];
        const size_t archiveIndex = encodedIndex - 1U;
        if (entry.crc == crc && archiveIndex < sArchive->Entries().size() &&
            std::string_view(sArchive->Entries()[archiveIndex]) == path) {
            sResourceNames.Remember(path,encodedIndex);
            return &entry;
        }
        slot = (slot + 1U) & (sCrcSlots.size() - 1U);
    }
    return nullptr;
}

bool IsSoundPath(std::string_view path) {
    return path.rfind("sound/", 0) == 0;
}

bool IsStreamingTexturePath(std::string_view path) {
    return path.rfind("textures/karts/", 0) == 0;
}

class SerializedReadLease {
  public:
    SerializedReadLease(size_t archiveIndex, std::string_view path) {
        if (sArchive == nullptr || sReadScratchInUse || IsSoundPath(path)) {
            return;
        }
        size_t byteCount = 0;
        if (sArchive->GetEntryUncompressedSizeByIndex(archiveIndex, &byteCount) !=
                mk64_3ds::O2rReadResult::Ok ||
            byteCount > kReadScratchLimit) {
            return;
        }
        sReadScratchInUse = true;
        mBytes = &sReadScratch;
        mUsesScratch = true;
    }

    ~SerializedReadLease() {
        if (mUsesScratch) {
            sReadScratch.clear();
            sReadScratchInUse = false;
        }
    }

    std::vector<uint8_t>* Bytes() {
        return mBytes;
    }

  private:
    std::vector<uint8_t> mLocalBytes;
    std::vector<uint8_t>* mBytes = &mLocalBytes;
    bool mUsesScratch = false;
};

LoadedResource* LoadResolvedPathOnce(size_t archiveIndex, CrcEntry* crcEntry) {
    if (sArchive == nullptr || archiveIndex >= sArchive->Entries().size() ||
        archiveIndex >= sLoadedResources.size()) {
        return nullptr;
    }
    if (crcEntry != nullptr && crcEntry->loaded != nullptr) {
        return crcEntry->loaded;
    }
    if (sLoadedResources[archiveIndex] != nullptr) {
        if (crcEntry != nullptr) {
            crcEntry->loaded = sLoadedResources[archiveIndex].get();
        }
        return sLoadedResources[archiveIndex].get();
    }

    const std::string& path = sArchive->Entries()[archiveIndex];
    if (Mk64Diagnostics3DSSetResource != nullptr) {
        Mk64Diagnostics3DSSetResource(path.c_str(), sLoadedResourceCount);
    }

    SerializedReadLease readLease(archiveIndex, path);
    const uint64_t readStart = Mk64Perf3DSResourceStart != nullptr ? Mk64Perf3DSResourceStart() : 0;
    const uint64_t physicalBefore=sArchive->PhysicalReadBytes();
    const auto readResult = sArchive->ReadEntryByIndex(archiveIndex, readLease.Bytes());
    sLoadEvents[sLoadEventNext]={readStart,static_cast<uint32_t>(archiveIndex),
        static_cast<uint32_t>(readLease.Bytes()->size()),static_cast<uint32_t>(sArchive->PhysicalReadBytes()-physicalBefore)};
    sLoadEventNext=(sLoadEventNext+1)%sLoadEvents.size();
    if(sLoadEventCount<sLoadEvents.size())++sLoadEventCount;
    if (Mk64Perf3DSResourceRead != nullptr) Mk64Perf3DSResourceRead(readStart, readLease.Bytes()->size());
    if (readResult != mk64_3ds::O2rReadResult::Ok) {
        return nullptr;
    }

    auto resource = std::make_unique<LoadedResource>();
    resource->archiveIndex = archiveIndex;
    resource->crcEntry = crcEntry;
    if (!ParseResource(*readLease.Bytes(), *resource)) {
        return nullptr;
    }

    LoadedResource* result = resource.get();
    if(resource->type==kTypeDisplayList && resource->pointer) {
        const auto item=std::make_pair(reinterpret_cast<uintptr_t>(resource->pointer),archiveIndex);
        sDisplayListAddresses.insert(std::lower_bound(sDisplayListAddresses.begin(),sDisplayListAddresses.end(),item),item);
    }
    sLoadedResources[archiveIndex] = std::move(resource);
    ++sLoadedResourceCount;
    if (crcEntry != nullptr) {
        crcEntry->loaded = result;
    }
    if (Mk64Diagnostics3DSSetResource != nullptr) {
        Mk64Diagnostics3DSSetResource(path.c_str(), sLoadedResourceCount);
    }
    return result;
}

LoadedResource* LoadResolvedPath(size_t archiveIndex, CrcEntry* crcEntry) {
    if (Mk64GameAudio3DSServiceLoading != nullptr) Mk64GameAudio3DSServiceLoading();
    try {
        return LoadResolvedPathOnce(archiveIndex, crcEntry);
    } catch (const std::bad_alloc&) {
        // Retry after each optional tier independently. Artwork's compressed
        // mirror/pages/decoded owners go first, preserving O2R residency when
        // they satisfy the request. Each tier is tried at most once: a failing
        // retry can populate a hot cache again and must not cause an endless
        // reclaim/reallocate loop. Required game state and live leases stay.
        uint32_t allowed = UINT32_MAX;
        for (unsigned attempt = 0; attempt < 5; ++attempt) {
            const auto released = ReclaimOptionalCaches(allowed);
            if (released == ReclaimedCache::None) break;
            allowed &= ~(1u << static_cast<unsigned>(released));
            if (Mk64Diagnostics3DSCheckpoint) {
                Mk64Diagnostics3DSCheckpoint(Mk64Resource3DSLastReclaimedCache());
            }
            try { return LoadResolvedPathOnce(archiveIndex, crcEntry); }
            catch (const std::bad_alloc&) { /* Try the next optional tier. */ }
        }
        throw;
    }
}

LoadedResource* LoadByPath(std::string_view path) {
    CrcEntry* entry = FindCrcEntry(path);
    return entry == nullptr ? nullptr : LoadResolvedPath(CrcEntryArchiveIndex(entry), entry);
}

LoadedResource* LoadByCrc(uint64_t crc) {
    CrcEntry* entry = FindCrcEntry(crc);
    return entry == nullptr ? nullptr : LoadResolvedPath(CrcEntryArchiveIndex(entry), entry);
}

template <typename Loader>
Mk64ResourceLoadResult3DS ResolveResource(Loader&& loader, LoadedResource** outResource) noexcept {
    if (outResource == nullptr) {
        return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
    }
    *outResource = nullptr;
    try {
        *outResource = loader();
        return *outResource != nullptr ? MK64_RESOURCE_LOAD_OK_3DS
                                       : MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
    } catch (const std::bad_alloc&) {
        return MK64_RESOURCE_LOAD_OUT_OF_MEMORY_3DS;
    } catch (...) {
        return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
    }
}

void RequestEviction(size_t archiveIndex);

LoadedResource* RetainedLookup(uint32_t archiveIndex) {
    return archiveIndex < sLoadedResources.size() ? sLoadedResources[archiveIndex].get() : nullptr;
}

bool EvictReleasedTexture(uint32_t archiveIndex, LoadedResource& resource) {
    if (resource.textureOwners != 0 || !resource.evictWhenUnused) return false;
    RequestEviction(archiveIndex);
    return true;
}

bool ReleaseRetainedTextures() {
    return sRetainedTextures.EvictOldest(0, 0, RetainedLookup, EvictReleasedTexture);
}

void RetainOrEvict(size_t archiveIndex, LoadedResource& resource) {
    if (!sRetainedTextures.Retain(static_cast<uint32_t>(archiveIndex), resource,
                                  RetainedLookup, EvictReleasedTexture)) {
        RequestEviction(archiveIndex);
    }
}

void RequestEviction(size_t archiveIndex) {
    if (sArchive == nullptr || archiveIndex >= sLoadedResources.size()) {
        return;
    }
    LoadedResource* resource = sLoadedResources[archiveIndex].get();
    if (resource == nullptr || archiveIndex >= sArchive->Entries().size() ||
        IsSoundPath(sArchive->Entries()[archiveIndex])) {
        return;
    }
    sRetainedTextures.Forget(*resource);
    resource->evictWhenUnused = true;
    resource->evictionRequested = true;
    if (resource->crcEntry != nullptr && resource->crcEntry->loaded == resource) {
        resource->crcEntry->loaded = nullptr;
    }
    if (resource->textureOwners != 0) {
        return;
    }
    if(resource->type==kTypeDisplayList) {
        const auto key=std::make_pair(reinterpret_cast<uintptr_t>(resource->pointer),archiveIndex);
        const auto found=std::lower_bound(sDisplayListAddresses.begin(),sDisplayListAddresses.end(),key);
        if(found!=sDisplayListAddresses.end() && *found==key)sDisplayListAddresses.erase(found);
    }
    sLoadedResources[archiveIndex].reset();
    --sLoadedResourceCount;
}

bool DirectoryContains(std::string_view directory, std::string_view path) {
    if (directory.empty() || path.size() < directory.size() ||
        path.substr(0, directory.size()) != directory) {
        return false;
    }
    return directory.back() == '/' || path.size() == directory.size() ||
           path[directory.size()] == '/';
}

bool TrackResourceDirectory(std::string_view path, std::string_view* directory) {
    constexpr std::array<std::string_view, 3> prefixes = {
        "models/tracks/", "textures/tracks/", "other/tracks/"
    };
    for (const std::string_view prefix : prefixes) {
        if (path.rfind(prefix, 0) != 0) {
            continue;
        }
        const std::string_view remainder = path.substr(prefix.size());
        const size_t slash = remainder.find('/');
        if (remainder.empty() || slash == 0 || slash == std::string_view::npos) {
            return false;
        }
        *directory = remainder.substr(0, slash);
        return true;
    }
    return false;
}

bool CeremonyResource(std::string_view path) {
    constexpr std::array<std::string_view, 3> prefixes = {
        "models/ceremony_data/", "textures/ceremony_data/", "other/ceremony_data/"
    };
    return std::any_of(prefixes.begin(), prefixes.end(), [path](std::string_view prefix) {
        return path.rfind(prefix, 0) == 0;
    });
}

bool RaceResource(std::string_view path) {
    std::string_view directory;
    if (TrackResourceDirectory(path, &directory) || CeremonyResource(path)) return true;
    // Raw clouds/Lakitu palettes belong to the objects destroyed at load_track.
    // Menu images, startup geometry and the pinned audio graph stay intact.
    constexpr std::array<std::string_view, 3> prefixes = {
        "textures/karts/", "textures/other_textures/", "textures/common_data/"
    };
    return std::any_of(prefixes.begin(), prefixes.end(), [path](std::string_view prefix) {
        return path.rfind(prefix, 0) == 0;
    });
}

void EvictDirectory(const char* name) {
    if (sArchive == nullptr) {
        return;
    }
    const std::string_view directory = NormalizePath(name);
    if (directory.empty() || IsSoundPath(directory)) {
        return;
    }
    for (size_t archiveIndex = 0; archiveIndex < sLoadedResources.size(); ++archiveIndex) {
        if (sLoadedResources[archiveIndex] != nullptr &&
            DirectoryContains(directory, sArchive->Entries()[archiveIndex])) {
            LoadedResource* resource = sLoadedResources[archiveIndex].get();
            if (resource->type == kTypeTexture && resource->pointer != nullptr &&
                Mk64Graphics3DSEvictSourceTexture != nullptr) {
                Mk64Graphics3DSEvictSourceTexture(resource->pointer);
            }
            RequestEviction(archiveIndex);
        }
    }
}

uint64_t TextureLifetimeToken(size_t archiveIndex) {
    if (archiveIndex >= UINT32_MAX || sRuntimeGeneration == 0) {
        return 0;
    }
    return (static_cast<uint64_t>(sRuntimeGeneration) << 32U) |
           static_cast<uint32_t>(archiveIndex + 1U);
}

bool FillTextureResult(LoadedResource* resource, Mk64TextureResource3DS* outTexture) {
    if (outTexture == nullptr) {
        return false;
    }
    *outTexture = {};
    if (resource == nullptr || resource->type != kTypeTexture || resource->pointer == nullptr ||
        resource->pointerSize == 0 || resource->textureWidth == 0 || resource->textureHeight == 0) {
        return false;
    }
    outTexture->data = static_cast<const uint8_t*>(resource->pointer);
    outTexture->size = resource->pointerSize;
    outTexture->width = resource->textureWidth;
    outTexture->height = resource->textureHeight;
    outTexture->type = resource->textureType;
    if (sArchive != nullptr && resource->archiveIndex < sArchive->Entries().size()) {
        outTexture->canonicalName = sArchive->Entries()[resource->archiveIndex].c_str();
    }
    return true;
}

} // namespace

extern "C" bool Mk64Resource3DSPrefetchKart(const char* name, size_t* cachedBytes) {
    if (sArchive == nullptr || name == nullptr) return false;
    const auto path = NormalizePath(name);
    if (!IsStreamingTexturePath(path)) return false;
    CrcEntry* entry = FindCrcEntry(path);
    bool ok = false;
    // Two MiB is bounded independently of kart texture/GPU ownership. Other
    // angles still use normal validated ZIP reads when first encountered.
    if (entry != nullptr) {
        ok = sArchive->CacheEntryByIndex(CrcEntryArchiveIndex(entry), mk64_3ds::kStreamingCacheBytes);
    }
    if (cachedBytes != nullptr) *cachedBytes = sArchive->CachedBytes();
    return ok;
}

extern "C" size_t Mk64Resource3DSPrefetchNames(const char* const* names, size_t count, size_t budget) {
    if (!sArchive || !names) return 0;
    budget = std::min(budget, mk64_3ds::kStreamingCacheBytes);
    size_t prepared = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!names[i]) continue;
        const auto path = NormalizePath(names[i]);
        // Audio remains pinned during init; never touch its state alongside
        // the producer. These are graphics/game dependencies only.
        if (IsSoundPath(path)) continue;
        const auto* entry = FindCrcEntry(path);
        if (entry && sArchive->CacheEntryByIndex(CrcEntryArchiveIndex(entry), budget)) ++prepared;
    }
    return prepared;
}

extern "C" void Mk64Resource3DSGetCacheStats(Mk64ResourceCacheStats3DS* stats) {
    if (!stats) return;
    *stats = {};
    if (sArchive) {
        const auto stream = sArchive->StreamingCacheStats();
        stats->streamEntries = stream.entries;
        stats->streamPayloadBytes = stream.payloadBytes;
        stats->streamAllocationBytes = stream.allocationBytes;
        stats->streamHits = stream.hits; stats->streamMisses = stream.misses;
        stats->streamByteEvictions = stream.byteEvictions;
    }
    stats->retentionStaleRemoved = sRetainedTextures.StaleRecordsRemoved();
    stats->retentionByteEvictions = sRetainedTextures.ByteEvictions();
    stats->retentionCountEvictions = sRetainedTextures.CountEvictions();
}

extern "C" bool Mk64Resource3DSMakeResident(size_t budget, void (*progress)(unsigned)) {
    if (sArchive == nullptr) return false;
    const bool ready = sArchive->MakeResident(budget, progress);
    // Also applies after a later pressure-triggered DropResident. Reuse the
    // existing 2 MiB kart-prefetch budget; do not reserve a second cache.
    sArchive->SetStreamingCacheBudget(mk64_3ds::kStreamingCacheBytes);
    return ready;
}
extern "C" bool Mk64Resource3DSDropResidency(void) {
    return sArchive != nullptr && sArchive->DropResident();
}
extern "C" bool Mk64Resource3DSWriteLoadHistory(const char* directory) {
    if(!directory || !sArchive)return false;
    char path[512];
    const int n=std::snprintf(path,sizeof path,"%s/resource-loads.csv",directory);
    if(n<0 || size_t(n)>=sizeof path)return false;
    FILE* file=std::fopen(path,"w");if(!file)return false;
    std::fprintf(file,"start_us,serialized_bytes,physical_SD_bytes,resource\n");
    for(size_t i=0;i<sLoadEventCount;++i) {
        const auto& e=sLoadEvents[(sLoadEventNext+sLoadEvents.size()-sLoadEventCount+i)%sLoadEvents.size()];
        if(e.index>=sArchive->Entries().size())continue;
        std::fprintf(file,"%llu,%lu,%lu,\"",static_cast<unsigned long long>(e.started),
            static_cast<unsigned long>(e.serializedBytes),static_cast<unsigned long>(e.physicalBytes));
        for(char c:sArchive->Entries()[e.index]){if(c=='\"')std::fputc('\"',file);std::fputc(c,file);}
        std::fprintf(file,"\"\n");
    }
    const bool loadsOk=std::ferror(file)==0;
    const bool loadsClosed=std::fclose(file)==0;
    const int m=std::snprintf(path,sizeof path,"%s/resource-lookup.txt",directory);
    if(m<0 || size_t(m)>=sizeof path)return false;
    file=std::fopen(path,"w");if(!file)return false;
    std::fprintf(file,"generation=%lu\nname_cache_bytes=%u\nname_cache_hits=%llu\nname_hash_misses=%llu\n",
        static_cast<unsigned long>(sRuntimeGeneration),unsigned(sizeof sResourceNames),
        static_cast<unsigned long long>(sResourceNameHits),static_cast<unsigned long long>(sResourceNameMisses));
    const bool lookupOk=std::ferror(file)==0;
    const bool lookupClosed=std::fclose(file)==0;
    return loadsOk && loadsClosed && lookupOk && lookupClosed;
}
extern "C" void Mk64Resource3DSSetStreamingCacheBudget(size_t budget) {
    if (sArchive) sArchive->SetStreamingCacheBudget(budget);
}
namespace {
ReclaimedCache ReclaimOptionalCaches(uint32_t allowed) {
    if (sReclaimingCaches) return ReclaimedCache::None;
    struct Scope { Scope(){sReclaimingCaches=true;} ~Scope(){sReclaimingCaches=false;} } scope;
    sLastReclaimedCache = ReclaimedCache::None;
    const auto permits = [allowed](ReclaimedCache cache) {return (allowed & (1u << static_cast<unsigned>(cache))) != 0;};
    if (permits(ReclaimedCache::Improved) && Mk64Improved3DSReclaim && Mk64Improved3DSReclaim()) {
        return sLastReclaimedCache = ReclaimedCache::Improved;
    }
    if (permits(ReclaimedCache::Retained) && ReleaseRetainedTextures()) {
        return sLastReclaimedCache = ReclaimedCache::Retained;
    }
    if (permits(ReclaimedCache::Native) && Mk64NativeKartReclaimCache && Mk64NativeKartReclaimCache()) {
        return sLastReclaimedCache = ReclaimedCache::Native;
    }
    if (sArchive) {
        const auto kind = sArchive->IsResident() ? ReclaimedCache::Archive : ReclaimedCache::Streaming;
        if (permits(kind) && sArchive->ReclaimPerformanceCache()) return sLastReclaimedCache = kind;
    }
    return ReclaimedCache::None;
}
}
extern "C" bool Mk64Resource3DSReclaimPerformanceCache(void) {
    return ReclaimOptionalCaches(UINT32_MAX) != ReclaimedCache::None;
}
extern "C" const char* Mk64Resource3DSLastReclaimedCache(void) {
    switch (sLastReclaimedCache) {
        case ReclaimedCache::Improved: return "improved-cache-released-for-memory";
        case ReclaimedCache::Retained: return "retained-textures-released-for-memory";
        case ReclaimedCache::Native: return "native-cache-released-for-memory";
        case ReclaimedCache::Archive: return "archive-residency-released-for-memory";
        case ReclaimedCache::Streaming: return "streaming-cache-released-for-memory";
        default: return "no-cache-released-for-memory";
    }
}
extern "C" bool Mk64Resource3DSResidencyMemoryLimited(void) {
    return sArchive && (sArchive->ResidentResult() == mk64_3ds::O2rResidentResult::BudgetExceeded ||
        sArchive->ResidentResult() == mk64_3ds::O2rResidentResult::AllocationFailed);
}
extern "C" const char* Mk64Resource3DSResidencyStatus(void) {
    if (!sArchive) return "archive-not-open";
    switch (sArchive->ResidentResult()) {
        case mk64_3ds::O2rResidentResult::Ready: return "ready";
        case mk64_3ds::O2rResidentResult::Released: return "released-for-memory";
        case mk64_3ds::O2rResidentResult::BudgetExceeded: return "budget-exceeded";
        case mk64_3ds::O2rResidentResult::AllocationFailed: return "allocation-failed";
        case mk64_3ds::O2rResidentResult::ReadFailed: return "archive-read-failed";
        default: return "archive-not-open";
    }
}
extern "C" size_t Mk64Resource3DSResidentRequiredBytes(void) {
    return sArchive ? sArchive->ResidentRequiredBytes() : 0;
}
extern "C" bool Mk64Resource3DSIsResident(void) {
    return sArchive != nullptr && sArchive->IsResident();
}
extern "C" uint64_t Mk64Resource3DSArchiveBytes(void) {
    return sArchive == nullptr ? 0 : sArchive->ArchiveBytes();
}
extern "C" uint64_t Mk64Resource3DSPhysicalReadCalls(void) {
    return sArchive == nullptr ? 0 : sArchive->PhysicalReadCalls();
}
extern "C" uint64_t Mk64Resource3DSPhysicalReadBytes(void) {
    return sArchive == nullptr ? 0 : sArchive->PhysicalReadBytes();
}
extern "C" size_t Mk64Resource3DSStreamingCachedBytes(void) {
    return sArchive == nullptr ? 0 : sArchive->CachedBytes();
}

extern "C" uint32_t Mk64Resource3DSGeneration(void) {
    return sArchive == nullptr ? 0 : sRuntimeGeneration;
}

extern "C" bool Mk64Resource3DSInit(const char* archivePath) {
    try {
        Mk64Resource3DSShutdown();
        if (archivePath == nullptr || archivePath[0] == '\0') {
            return false;
        }
        auto archive = std::make_unique<mk64_3ds::O2rArchiveReader>(archivePath);
        if (archive->Open() != mk64_3ds::O2rReadResult::Ok) {
            return false;
        }
        const size_t entryCount = archive->Entries().size();
        if (entryCount > UINT32_MAX || entryCount > SIZE_MAX / 2U) {
            return false;
        }
        size_t slotCount = 1;
        while (slotCount < std::max<size_t>(2U, entryCount * 2U)) {
            if (slotCount > SIZE_MAX / 2U) {
                return false;
            }
            slotCount *= 2U;
        }
        sCrcEntries.resize(entryCount);
        sCrcSlots.assign(slotCount, 0);
        sResourceNames.Reset();
        sResourceNameHits=sResourceNameMisses=0;
        sLoadedResources.resize(entryCount);
        sRetainedTextures.Reset();
        sLoadEventNext=sLoadEventCount=0;
        for (size_t archiveIndex = 0; archiveIndex < entryCount; ++archiveIndex) {
            CrcEntry& entry = sCrcEntries[archiveIndex];
            entry.crc = PathCrc(archive->Entries()[archiveIndex]);
            size_t slot = CrcSlot(entry.crc);
            while (sCrcSlots[slot] != 0) {
                slot = (slot + 1U) & (sCrcSlots.size() - 1U);
            }
            sCrcSlots[slot] = static_cast<uint32_t>(archiveIndex + 1U);
        }
        ++sRuntimeGeneration;
        if (sRuntimeGeneration == 0) {
            ++sRuntimeGeneration;
        }
        sArchive = std::move(archive);
        if (!Mk64Resource3DSValidateCrcIndex()) {
            throw std::runtime_error("compact CRC index did not preserve archive lookup parity");
        }
        if (Mk64Diagnostics3DSSetArchiveEntryCount != nullptr) {
            Mk64Diagnostics3DSSetArchiveEntryCount(sArchive->Entries().size());
        }

        sBanksById.fill(SIZE_MAX);
        sSequencesById.fill(SIZE_MAX);
        {
            std::vector<uint8_t> bytes;
            for (size_t archiveIndex = 0; archiveIndex < sArchive->Entries().size(); ++archiveIndex) {
                const std::string& entry = sArchive->Entries()[archiveIndex];
                const bool isBank = entry.rfind("sound/banks/", 0) == 0;
                const bool isSequence = entry.rfind("sound/sequences/", 0) == 0;
                if ((!isBank && !isSequence) ||
                    sArchive->ReadEntryByIndex(archiveIndex, &bytes) != mk64_3ds::O2rReadResult::Ok ||
                    bytes.size() < kOtrHeaderSize + sizeof(uint32_t)) {
                    continue;
                }
                Reader header(bytes, 4);
                Reader body(bytes, kOtrHeaderSize);
                uint32_t type = 0;
                uint32_t version = 0;
                uint32_t id = 0;
                if (!header.ReadU32(&type) || !header.ReadU32(&version) || version != 0 ||
                    !body.ReadU32(&id) || id > UINT8_MAX) {
                    continue;
                }
                if (isBank && type == kTypeAudioBank) {
                    sBanksById[static_cast<uint8_t>(id)] = archiveIndex;
                } else if (isSequence && type == kTypeSequence) {
                    sSequencesById[static_cast<uint8_t>(id)] = archiveIndex;
                }
            }
        }

        // Audio synthesis runs on a second ARM11 core when one is available.
        // Resolve every bank, sequence and recursively referenced sample now,
        // while initialization is still single-threaded. The mixer can then
        // use immutable pointer tables without touching the non-concurrent
        // O2R FILE* or resource cache alongside Fast3D.
        for (size_t bankId = 0; bankId < sBanksById.size(); ++bankId) {
            const size_t archiveIndex = sBanksById[bankId];
            if (archiveIndex == SIZE_MAX) {
                continue;
            }
            LoadedResource* resource =
                LoadResolvedPath(archiveIndex, &sCrcEntries[archiveIndex]);
            if (resource == nullptr || resource->type != kTypeAudioBank ||
                resource->pointer == nullptr) {
                throw std::runtime_error("could not preload an audio bank");
            }
            auto* bank = static_cast<CtlEntry*>(resource->pointer);
            if (bank->bankId != bankId) {
                throw std::runtime_error("audio bank id did not match its archive index");
            }
            sPinnedBanks[bankId] = bank;
        }
        for (size_t sequenceId = 0; sequenceId < sSequencesById.size(); ++sequenceId) {
            const size_t archiveIndex = sSequencesById[sequenceId];
            if (archiveIndex == SIZE_MAX) {
                continue;
            }
            LoadedResource* resource =
                LoadResolvedPath(archiveIndex, &sCrcEntries[archiveIndex]);
            if (resource == nullptr || resource->type != kTypeSequence ||
                resource->pointer == nullptr) {
                throw std::runtime_error("could not preload an audio sequence");
            }
            auto* sequence = static_cast<AudioSequenceData*>(resource->pointer);
            if (sequence->id != sequenceId) {
                throw std::runtime_error("audio sequence id did not match its archive index");
            }
            sPinnedSequences[sequenceId] = sequence;
            ++sPinnedSequenceCount;
        }

        // The fixed pointer tables are the only audio lookup state needed at
        // runtime. Clear the temporary archive-index tables before graphics.
        sBanksById.fill(SIZE_MAX);
        sSequencesById.fill(SIZE_MAX);
        return true;
    } catch (const std::bad_alloc&) {
        if (Mk64Diagnostics3DSFailure != nullptr) {
            Mk64Diagnostics3DSFailure("resource-runtime-init", "out of memory while indexing mk64.o2r");
        }
        Mk64Resource3DSShutdown();
        return false;
    } catch (...) {
        if (Mk64Diagnostics3DSFailure != nullptr) {
            Mk64Diagnostics3DSFailure("resource-runtime-init", "unexpected exception while indexing mk64.o2r");
        }
        Mk64Resource3DSShutdown();
        return false;
    }
}

extern "C" void Mk64Resource3DSShutdown(void) {
    decltype(sDisplayListAddresses){}.swap(sDisplayListAddresses);
    sPinnedBanks.fill(nullptr);
    sPinnedSequences.fill(nullptr);
    sPinnedSequenceCount = 0;
    decltype(sLoadedResources){}.swap(sLoadedResources);
    sRetainedTextures.Reset();
    decltype(sCrcEntries){}.swap(sCrcEntries);
    decltype(sCrcSlots){}.swap(sCrcSlots);
    sResourceNames.Reset();
    sLoadedResourceCount = 0;
    sBanksById.fill(SIZE_MAX);
    sSequencesById.fill(SIZE_MAX);
    decltype(sReadScratch){}.swap(sReadScratch);
    sReadScratchInUse = false;
    sArchive.reset();
    if (AudioDma_Clear != nullptr) {
        AudioDma_Clear();
    }
}

extern "C" size_t Mk64Resource3DSArchiveEntryCount(void) {
    return sArchive == nullptr ? 0 : sArchive->Entries().size();
}

extern "C" size_t Mk64Resource3DSLoadedCount(void) {
    return sLoadedResourceCount;
}

extern "C" void Mk64Resource3DSGetRaceStats(Mk64RaceResourceStats3DS* stats) {
    if (stats == nullptr) return;
    *stats = {};
    if (sArchive == nullptr) return;
    for (size_t index = 0; index < sLoadedResources.size(); ++index) {
        const auto* resource = sLoadedResources[index].get();
        if (resource == nullptr || !RaceResource(sArchive->Entries()[index])) continue;
        ++stats->entries;
        stats->bytes += resource->allocatedBytes;
        stats->kartEntries += IsStreamingTexturePath(sArchive->Entries()[index]);
        stats->rawEntries += resource->rawBorrowed;
        stats->ownedEntries += resource->textureOwners != 0;
    }
}

extern "C" size_t Mk64Resource3DSReleaseRaceResources(void) {
    if (sArchive == nullptr) return 0;
    size_t requested = 0;
    for (size_t index = 0; index < sLoadedResources.size(); ++index) {
        if (sLoadedResources[index] == nullptr || !RaceResource(sArchive->Entries()[index])) continue;
        RequestEviction(index);
        ++requested;
    }
    return requested;
}

extern "C" size_t Mk64Resource3DSSetActiveTrack(const char* assetDirectory) {
    if (sArchive == nullptr || assetDirectory == nullptr || assetDirectory[0] == '\0') {
        return 0;
    }
    const std::string_view active(assetDirectory);
    if (active.find('/') != std::string_view::npos || active.find('\\') != std::string_view::npos) {
        return 0;
    }
    const bool podium = active == "podium_ceremony";
    size_t requested = 0;
    for (size_t archiveIndex = 0; archiveIndex < sLoadedResources.size(); ++archiveIndex) {
        LoadedResource* resource = sLoadedResources[archiveIndex].get();
        if (resource == nullptr) {
            continue;
        }
        const std::string_view path = sArchive->Entries()[archiveIndex];
        std::string_view directory;
        const bool trackResource = TrackResourceDirectory(path, &directory);
        const bool ceremonyResource = CeremonyResource(path);
        const bool keepTrack = trackResource &&
            (directory == active || (podium && directory == "royal_raceway"));
        const bool keepCeremony = ceremonyResource && podium;
        if ((!trackResource && !ceremonyResource) || keepTrack || keepCeremony) {
            continue;
        }
        if (resource->type == kTypeTexture && resource->pointer != nullptr &&
            Mk64Graphics3DSEvictSourceTexture != nullptr) {
            Mk64Graphics3DSEvictSourceTexture(resource->pointer);
        }
        RequestEviction(archiveIndex);
        ++requested;
    }
    return requested;
}

extern "C" bool Mk64Resource3DSValidateCrcIndex(void) {
    if (sArchive == nullptr || sCrcSlots.empty() ||
        (sCrcSlots.size() & (sCrcSlots.size() - 1U)) != 0 ||
        sCrcEntries.size() != sArchive->Entries().size() ||
        sLoadedResources.size() != sArchive->Entries().size()) {
        return false;
    }
    for (size_t archiveIndex = 0; archiveIndex < sCrcEntries.size(); ++archiveIndex) {
        const CrcEntry& entry = sCrcEntries[archiveIndex];
        const std::string& path = sArchive->Entries()[archiveIndex];
        if (entry.crc != PathCrc(path)) {
            return false;
        }

        bool exactSlotFound = false;
        size_t slot = CrcSlot(entry.crc);
        for (size_t probe = 0; probe < sCrcSlots.size(); ++probe) {
            const uint32_t encodedIndex = sCrcSlots[slot];
            if (encodedIndex == 0) {
                break;
            }
            if (encodedIndex - 1U == archiveIndex) {
                exactSlotFound = true;
                break;
            }
            slot = (slot + 1U) & (sCrcSlots.size() - 1U);
        }
        CrcEntry* firstByCrc = FindCrcEntry(entry.crc);
        CrcEntry* firstByPath = FindCrcEntry(path);
        if (!exactSlotFound || firstByCrc == nullptr || firstByPath == nullptr ||
            CrcEntryArchiveIndex(firstByCrc) > archiveIndex ||
            CrcEntryArchiveIndex(firstByPath) > archiveIndex ||
            std::string_view(sArchive->Entries()[CrcEntryArchiveIndex(firstByPath)]) != path) {
            return false;
        }
    }
    return true;
}

extern "C" bool Mk64Resource3DSGetTexture(const char* name, Mk64TextureResource3DS* outTexture) {
    if (outTexture != nullptr) {
        *outTexture = {};
    }
    LoadedResource* resource = nullptr;
    return ResolveResource([name] { return LoadByPath(NormalizePath(name)); }, &resource) ==
               MK64_RESOURCE_LOAD_OK_3DS &&
           FillTextureResult(resource, outTexture);
}

extern "C" Mk64ResourceLoadResult3DS Mk64Resource3DSAcquireTexture(
    const char* name, Mk64TextureResource3DS* outTexture) {
    LoadedResource* resource = nullptr;
    const Mk64ResourceLoadResult3DS loadResult = ResolveResource(
        [name] { return LoadByPath(NormalizePath(name)); }, &resource);
    if (loadResult != MK64_RESOURCE_LOAD_OK_3DS) {
        if (outTexture != nullptr) {
            *outTexture = {};
        }
        return loadResult;
    }
    if (!FillTextureResult(resource, outTexture) || resource->textureOwners == UINT32_MAX) {
        if (outTexture != nullptr) {
            *outTexture = {};
        }
        return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
    }
    const uint64_t token = TextureLifetimeToken(resource->archiveIndex);
    if (token == 0) {
        *outTexture = {};
        return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
    }
    sRetainedTextures.Forget(*resource);
    ++resource->textureOwners;
    // Every texture used exclusively through ResourceManager has an explicit
    // shared owner in Fast3D. Reclaim its decoded source as soon as the bounded
    // GPU cache drops the last owner. Legacy LOAD_ASSET_RAW callers receive a
    // bare pointer, so those resources are pinned until their normal directory
    // transition instead of risking a dangling pointer and an invisible kart.
    if (!resource->rawBorrowed) {
        resource->evictWhenUnused = true;
    }
    outTexture->lifetimeToken = token;
    return MK64_RESOURCE_LOAD_OK_3DS;
}

extern "C" Mk64ResourceLoadResult3DS Mk64Resource3DSResolveDataByName(
    const char* name, void** outData) {
    if (outData == nullptr) {
        return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
    }
    *outData = nullptr;
    LoadedResource* resource = nullptr;
    const Mk64ResourceLoadResult3DS result = ResolveResource(
        [name] { return LoadByPath(NormalizePath(name)); }, &resource);
    if (result == MK64_RESOURCE_LOAD_OK_3DS) {
        if (resource->type == kTypeTexture) {
            resource->rawBorrowed = true;
            resource->evictWhenUnused = false;
        }
        *outData = resource->pointer;
    }
    return result;
}

extern "C" Mk64ResourceLoadResult3DS Mk64Resource3DSResolveDataByCrc(
    uint64_t crc, void** outData) {
    if (outData == nullptr) {
        return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
    }
    *outData = nullptr;
    LoadedResource* resource = nullptr;
    const Mk64ResourceLoadResult3DS result = ResolveResource(
        [crc] { return LoadByCrc(crc); }, &resource);
    if (result == MK64_RESOURCE_LOAD_OK_3DS) {
        if (resource->type == kTypeTexture) {
            resource->rawBorrowed = true;
            resource->evictWhenUnused = false;
        }
        *outData = resource->pointer;
    }
    return result;
}

extern "C" Mk64ResourceLoadResult3DS Mk64Resource3DSAcquireDataByCrc(
        uint64_t crc,void** outData,uint64_t* lifetimeToken) {
    if(outData)*outData=nullptr;if(lifetimeToken)*lifetimeToken=0;
    if(!outData || !lifetimeToken)return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
    LoadedResource* resource=nullptr;
    const auto result=ResolveResource([&](){return LoadByCrc(crc);},&resource);
    if(result!=MK64_RESOURCE_LOAD_OK_3DS)return result;
    if(!resource || !resource->pointer)return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
    if(resource->textureOwners==UINT32_MAX)return MK64_RESOURCE_LOAD_OUT_OF_MEMORY_3DS;
    const auto token=TextureLifetimeToken(resource->archiveIndex);
    if(!token)return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
    sRetainedTextures.Forget(*resource);
    ++resource->textureOwners; // Shared decoded-resource pin count, also used by textures.
    *outData=resource->pointer;*lifetimeToken=token;return MK64_RESOURCE_LOAD_OK_3DS;
}
extern "C" void Mk64Resource3DSReleaseData(uint64_t token) {Mk64Resource3DSReleaseTexture(token);}

extern "C" bool Mk64Resource3DSDisplayListIdentity(const void* pointer,uint64_t* hash,size_t* entry) {
    if(hash)*hash=0;if(entry)*entry=0;
    if(!pointer || !hash || !entry)return false;
    const auto address=reinterpret_cast<uintptr_t>(pointer);
    auto found=std::upper_bound(sDisplayListAddresses.begin(),sDisplayListAddresses.end(),address,
        [](uintptr_t key,const auto& item){return key<item.first;});
    if(found==sDisplayListAddresses.begin())return false;
    --found;
    const auto* resource=sLoadedResources[found->second].get();
    const auto offset=address-found->first;
    if(!resource || !resource->crcEntry || offset>=resource->pointerSize || offset%sizeof(Gfx))return false;
    *hash=resource->crcEntry->crc;*entry=offset/sizeof(Gfx);return true;
}

extern "C" void Mk64Resource3DSReleaseTexture(uint64_t lifetimeToken) {
    const uint32_t generation = static_cast<uint32_t>(lifetimeToken >> 32U);
    const uint32_t encodedIndex = static_cast<uint32_t>(lifetimeToken);
    if (generation == 0 || generation != sRuntimeGeneration || encodedIndex == 0) {
        return;
    }
    const size_t archiveIndex = static_cast<size_t>(encodedIndex - 1U);
    if (archiveIndex >= sLoadedResources.size()) {
        return;
    }
    LoadedResource* resource = sLoadedResources[archiveIndex].get();
    if (resource == nullptr || resource->textureOwners == 0) {
        return;
    }
    --resource->textureOwners;
    if (resource->textureOwners == 0 && resource->evictWhenUnused) {
        if (resource->evictionRequested) {
            RequestEviction(archiveIndex);
        } else {
            RetainOrEvict(archiveIndex, *resource);
        }
    }
}

extern "C" bool Mk64Resource3DSGetArchiveEntrySizeByName(const char* name, size_t* byteCount) {
    if (byteCount == nullptr) {
        return false;
    }
    *byteCount = 0;
    if (sArchive == nullptr) {
        return false;
    }
    const std::string_view path = NormalizePath(name);
    if (path.empty()) {
        return false;
    }
    CrcEntry* entry = FindCrcEntry(path);
    if (entry == nullptr) {
        return false;
    }
    return sArchive->GetEntryUncompressedSizeByIndex(CrcEntryArchiveIndex(entry), byteCount) ==
           mk64_3ds::O2rReadResult::Ok;
}

extern "C" bool Mk64Resource3DSGetArchiveEntrySizeByCrc(uint64_t crc, size_t* byteCount) {
    if (byteCount == nullptr) {
        return false;
    }
    *byteCount = 0;
    if (sArchive == nullptr) {
        return false;
    }
    CrcEntry* entry = FindCrcEntry(crc);
    if (entry == nullptr) {
        return false;
    }
    return sArchive->GetEntryUncompressedSizeByIndex(CrcEntryArchiveIndex(entry), byteCount) ==
           mk64_3ds::O2rReadResult::Ok;
}

extern "C" uint64_t ResourceGetCrcByName(const char* name) {
    const std::string_view path = NormalizePath(name);
    if(path.empty())return 0;
    // Native procedure calls repeatedly name the same archive entries. Reuse
    // their indexed CRC, while preserving hashing of unknown/pre-init names.
    if(!sArchive || sCrcSlots.empty())return PathCrc(path);
    uint64_t crc=0;
    FindCrcEntry(path,&crc);
    return crc;
}

extern "C" const char* ResourceGetNameByCrc(uint64_t crc) {
    if (sArchive == nullptr) {
        return nullptr;
    }
    CrcEntry* entry = FindCrcEntry(crc);
    return entry == nullptr ? nullptr
                            : sArchive->Entries()[CrcEntryArchiveIndex(entry)].c_str();
}

extern "C" void* ResourceGetDataByName(const char* name) {
    if (Mk64NativeKartPaletteData) {
        if (const auto* prepared=Mk64NativeKartPaletteData(name)) {
            if (Mk64Perf3DSPreparedPaletteHit) Mk64Perf3DSPreparedPaletteHit();
            return const_cast<uint8_t*>(prepared);
        }
    }
    void* data = nullptr;
    Mk64Resource3DSResolveDataByName(name, &data);
    return data;
}

extern "C" void* ResourceGetDataByCrc(uint64_t crc) {
    void* data = nullptr;
    Mk64Resource3DSResolveDataByCrc(crc, &data);
    return data;
}

extern "C" size_t ResourceGetSizeByName(const char* name) {
    if (Mk64NativeKartPaletteData && Mk64NativeKartPaletteData(name)) return 128;
    LoadedResource* resource = nullptr;
    ResolveResource([name] { return LoadByPath(NormalizePath(name)); }, &resource);
    return resource == nullptr ? 0 : resource->pointerSize;
}

extern "C" size_t ResourceGetSizeByCrc(uint64_t crc) {
    LoadedResource* resource = nullptr;
    ResolveResource([crc] { return LoadByCrc(crc); }, &resource);
    return resource == nullptr ? 0 : resource->pointerSize;
}

extern "C" uint16_t ResourceGetTexWidthByName(const char* name) {
    LoadedResource* resource = nullptr;
    ResolveResource([name] { return LoadByPath(NormalizePath(name)); }, &resource);
    return resource == nullptr ? 0 : resource->textureWidth;
}

extern "C" uint16_t ResourceGetTexHeightByName(const char* name) {
    LoadedResource* resource = nullptr;
    ResolveResource([name] { return LoadByPath(NormalizePath(name)); }, &resource);
    return resource == nullptr ? 0 : resource->textureHeight;
}

extern "C" size_t ResourceGetTexSizeByName(const char* name) {
    return ResourceGetSizeByName(name);
}

extern "C" uint16_t ResourceGetTexWidthByCrc(uint64_t crc) {
    LoadedResource* resource = nullptr;
    ResolveResource([crc] { return LoadByCrc(crc); }, &resource);
    return resource == nullptr ? 0 : resource->textureWidth;
}

extern "C" uint16_t ResourceGetTexHeightByCrc(uint64_t crc) {
    LoadedResource* resource = nullptr;
    ResolveResource([crc] { return LoadByCrc(crc); }, &resource);
    return resource == nullptr ? 0 : resource->textureHeight;
}

extern "C" size_t ResourceGetTexSizeByCrc(uint64_t crc) {
    return ResourceGetSizeByCrc(crc);
}

extern "C" uint8_t ResourceGetIsCustomByName(const char*) {
    return 0;
}
extern "C" uint8_t ResourceGetIsCustomByCrc(uint64_t) {
    return 0;
}

extern "C" void ResourceDirtyByName(const char* name) {
    const std::string_view path = NormalizePath(name);
    if (path.empty()) {
        return;
    }
    // Banks, sequences and their recursively loaded samples back immutable
    // pointers used by the cross-core audio mixer. A generic future dirty or
    // unload request must never invalidate that pinned object graph.
    if (IsSoundPath(path)) {
        return;
    }
    CrcEntry* entry = FindCrcEntry(path);
    if (entry != nullptr) {
        const size_t archiveIndex = CrcEntryArchiveIndex(entry);
        LoadedResource* resource = archiveIndex < sLoadedResources.size()
                                       ? sLoadedResources[archiveIndex].get()
                                       : nullptr;
        if (resource != nullptr && resource->type == kTypeTexture &&
            resource->pointer != nullptr && Mk64Graphics3DSEvictSourceTexture != nullptr) {
            Mk64Graphics3DSEvictSourceTexture(resource->pointer);
        }
        RequestEviction(archiveIndex);
    }
}

extern "C" void ResourceDirtyByCrc(uint64_t crc) {
    CrcEntry* entry = FindCrcEntry(crc);
    if (entry != nullptr) {
        const size_t archiveIndex = CrcEntryArchiveIndex(entry);
        LoadedResource* resource = archiveIndex < sLoadedResources.size()
                                       ? sLoadedResources[archiveIndex].get()
                                       : nullptr;
        if (resource != nullptr && resource->type == kTypeTexture &&
            resource->pointer != nullptr && Mk64Graphics3DSEvictSourceTexture != nullptr) {
            Mk64Graphics3DSEvictSourceTexture(resource->pointer);
        }
        RequestEviction(archiveIndex);
    }
}

extern "C" void ResourceUnloadByName(const char* name) {
    ResourceDirtyByName(name);
}

extern "C" void ResourceUnloadByCrc(uint64_t crc) {
    ResourceDirtyByCrc(crc);
}

extern "C" void ResourceLoadDirectory(const char*) {
}
extern "C" void ResourceLoadDirectoryAsync(const char*) {
}
extern "C" void ResourceDirtyDirectory(const char* name) {
    EvictDirectory(name);
}
extern "C" void ResourceUnloadDirectory(const char* name) {
    EvictDirectory(name);
}

extern "C" uint32_t IsResourceManagerLoaded(void) {
    return sArchive != nullptr;
}

extern "C" bool GameEngine_OTRSigCheck(const char* data) {
    return data != nullptr && std::strncmp(data, kOtrSignature.data(), kOtrSignature.size()) == 0;
}

extern "C" int32_t GameEngine_ResourceGetTexTypeByName(const char* name) {
    LoadedResource* resource = nullptr;
    ResolveResource([name] { return LoadByPath(NormalizePath(name)); }, &resource);
    return resource == nullptr ? 0 : static_cast<int32_t>(resource->textureType);
}

extern "C" CtlEntry* GameEngine_LoadBank(uint8_t bankId) {
    return sPinnedBanks[bankId];
}

extern "C" uint8_t GameEngine_IsBankLoaded(uint8_t bankId) {
    return sPinnedBanks[bankId] != nullptr;
}

extern "C" void GameEngine_UnloadBank(uint8_t bankId) {
    // The vanilla audio loader calls this while reinitializing a player *after*
    // it has obtained pointers into the bank graph. Desktop SpaghettiKart only
    // clears its lookup-table slot here; ResourceManager keeps the underlying
    // object alive. Erasing our owning cache entry made those pointers dangle
    // immediately and the title sequence subsequently decoded freed memory.
    // Keep audio resources resident until the 3DS runtime has an ownership-aware
    // deferred eviction scheme.
    (void)bankId;
}

extern "C" AudioSequenceData* GameEngine_LoadSequence(uint8_t sequenceId) {
    return sPinnedSequences[sequenceId];
}

extern "C" uint32_t GameEngine_GetSequenceCount(void) {
    return sPinnedSequenceCount;
}

extern "C" uint8_t GameEngine_IsSequenceLoaded(uint8_t sequenceId) {
    return sPinnedSequences[sequenceId] != nullptr;
}

extern "C" void GameEngine_UnloadSequence(uint8_t sequenceId) {
    // See GameEngine_UnloadBank above. load_sequence_internal retains the data
    // pointer across init_sequence_player(), which invokes this callback.
    (void)sequenceId;
}
