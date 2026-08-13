#include "resource_runtime_3ds.h"

#include "o2r_archive_reader.hpp"

#define MK64_3DS_LIBULTRA_ONLY
#include <libultraship/libultra/gbi.h>
#include <ship/utils/StrHash64.h>

#include "audio/internal.h"
#include "waypoints.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#define MK64_OPTIONAL_SYMBOL __attribute__((weak_import))
#else
#define MK64_OPTIONAL_SYMBOL __attribute__((weak))
#endif
extern "C" void* linearAlloc(size_t size);
extern "C" void linearFree(void* mem);
extern "C" void Mk64Diagnostics3DSSetResource(const char*, size_t) MK64_OPTIONAL_SYMBOL;
extern "C" void Mk64Diagnostics3DSSetArchiveEntryCount(size_t) MK64_OPTIONAL_SYMBOL;
extern "C" void Mk64Diagnostics3DSFailure(const char*, const char*) MK64_OPTIONAL_SYMBOL;
extern "C" void AudioDma_Register(const void* base, size_t size) MK64_OPTIONAL_SYMBOL;
extern "C" void AudioDma_Clear(void) MK64_OPTIONAL_SYMBOL;

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

    bool ReadString(std::string* value) {
        int32_t count = 0;
        if (value == nullptr || !ReadS32(&count) || count < 0 || !CanRead(static_cast<size_t>(count))) {
            return false;
        }
        value->assign(reinterpret_cast<const char*>(mBytes.data() + mOffset), static_cast<size_t>(count));
        mOffset += static_cast<size_t>(count);
        return true;
    }

  private:
    const std::vector<uint8_t>& mBytes;
    size_t mOffset;
};

struct LoadedResource {
    ~LoadedResource() {
        for (void* allocation : allocations) {
            linearFree(allocation);
        }
    }

    template <typename T> T* Allocate(size_t count = 1) {
        if (count == 0 || count > SIZE_MAX / sizeof(T)) {
            return nullptr;
        }
        const size_t byteCount = sizeof(T) * count;
        void* bytes = linearAlloc(byteCount);
        if (bytes == nullptr) {
            return nullptr;
        }
        std::memset(bytes, 0, byteCount);
        T* result = reinterpret_cast<T*>(bytes);
        try {
            allocations.emplace_back(bytes);
        } catch (...) {
            linearFree(bytes);
            throw;
        }
        return result;
    }

    uint32_t type = 0;
    uint32_t version = 0;
    void* pointer = nullptr;
    size_t pointerSize = 0;
    uint16_t textureWidth = 0;
    uint16_t textureHeight = 0;
    uint32_t textureType = 0;
    std::vector<void*> allocations;
};

std::unique_ptr<mk64_3ds::O2rArchiveReader> sArchive;

struct CrcEntry {
    size_t archiveIndex = 0;
    LoadedResource* loaded = nullptr;
};

std::unordered_map<std::string, std::unique_ptr<LoadedResource>> sCache;
std::unordered_map<uint64_t, CrcEntry> sCrcToEntry;
std::unordered_map<uint8_t, std::string> sBanksById;
std::unordered_map<uint8_t, std::string> sSequencesById;
std::array<CtlEntry*, UINT8_MAX + 1U> sPinnedBanks = {};
std::array<AudioSequenceData*, UINT8_MAX + 1U> sPinnedSequences = {};
uint32_t sPinnedSequenceCount = 0;

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
    std::string sampleName;
    if (sound == nullptr || !reader.ReadString(&sampleName)) {
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
        std::string bankName;
        if (!reader.ReadString(&bankName)) {
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

LoadedResource* LoadResolvedPath(std::string_view path, size_t archiveIndex,
                                 CrcEntry* crcEntry) {
    if (sArchive == nullptr || path.empty()) {
        return nullptr;
    }
    if (crcEntry != nullptr && crcEntry->loaded != nullptr) {
        return crcEntry->loaded;
    }

    try {
    const std::string key(path);
    if (const auto found = sCache.find(key); found != sCache.end()) {
        if (crcEntry != nullptr) {
            crcEntry->loaded = found->second.get();
        }
        return found->second.get();
    }
    if (Mk64Diagnostics3DSSetResource != nullptr) {
        Mk64Diagnostics3DSSetResource(key.c_str(), sCache.size());
    }
    std::vector<uint8_t> bytes;
    try {
        const mk64_3ds::O2rReadResult readResult = archiveIndex < sArchive->Entries().size()
                                                         ? sArchive->ReadEntryByIndex(archiveIndex, &bytes)
                                                         : sArchive->ReadEntry(path, &bytes);
        if (readResult != mk64_3ds::O2rReadResult::Ok) {
            return nullptr;
        }
    } catch (const std::bad_alloc&) {
        return nullptr;
    }

    std::unique_ptr<LoadedResource> resource;
    try {
        resource = std::make_unique<LoadedResource>();
        if (!ParseResource(bytes, *resource)) {
            return nullptr;
        }
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
    LoadedResource* result = resource.get();
    sCache.emplace(key, std::move(resource));
    if (crcEntry != nullptr) {
        crcEntry->loaded = result;
    }
    if (Mk64Diagnostics3DSSetResource != nullptr) {
        Mk64Diagnostics3DSSetResource(key.c_str(), sCache.size());
    }
    return result;
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

LoadedResource* LoadByPath(std::string_view path) {
    if (sArchive == nullptr || path.empty()) {
        return nullptr;
    }
    if (const auto found = sCrcToEntry.find(PathCrc(path)); found != sCrcToEntry.end() &&
        found->second.archiveIndex < sArchive->Entries().size() &&
        std::string_view(sArchive->Entries()[found->second.archiveIndex]) == path) {
        return LoadResolvedPath(path, found->second.archiveIndex, &found->second);
    }
    // Preserve the name-based API as a collision-safe fallback for paths that
    // are not represented by the CRC index.
    return LoadResolvedPath(path, SIZE_MAX, nullptr);
}

LoadedResource* LoadByCrc(uint64_t crc) {
    if (sArchive == nullptr) {
        return nullptr;
    }
    const auto found = sCrcToEntry.find(crc);
    if (found == sCrcToEntry.end() || found->second.archiveIndex >= sArchive->Entries().size()) {
        return nullptr;
    }
    if (found->second.loaded != nullptr) {
        return found->second.loaded;
    }
    LoadedResource* resource =
        LoadResolvedPath(sArchive->Entries()[found->second.archiveIndex],
                         found->second.archiveIndex, &found->second);
    found->second.loaded = resource;
    return resource;
}

} // namespace

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
        sCrcToEntry.reserve(archive->Entries().size());
        for (size_t i = 0; i < archive->Entries().size(); ++i) {
            sCrcToEntry.emplace(PathCrc(archive->Entries()[i]), CrcEntry{ i, nullptr });
        }
        sArchive = std::move(archive);
        if (Mk64Diagnostics3DSSetArchiveEntryCount != nullptr) {
            Mk64Diagnostics3DSSetArchiveEntryCount(sArchive->Entries().size());
        }

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
            if (!header.ReadU32(&type) || !header.ReadU32(&version) || version != 0 || !body.ReadU32(&id) ||
                id > UINT8_MAX) {
                continue;
            }
            if (isBank && type == kTypeAudioBank) {
                sBanksById[static_cast<uint8_t>(id)] = entry;
            } else if (isSequence && type == kTypeSequence) {
                sSequencesById[static_cast<uint8_t>(id)] = entry;
            }
        }

        // Audio synthesis runs on a second ARM11 core when one is available.
        // Resolve every bank, sequence and recursively referenced sample now,
        // while initialization is still single-threaded. The mixer can then
        // use immutable pointer tables without touching the non-concurrent
        // O2R FILE*, resource cache or unordered maps alongside Fast3D.
        for (const auto& [bankId, path] : sBanksById) {
            LoadedResource* resource = LoadByPath(path);
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
        for (const auto& [sequenceId, path] : sSequencesById) {
            LoadedResource* resource = LoadByPath(path);
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
        // runtime. Release the temporary path maps and their buckets before
        // graphics begins loading thousands of course and kart resources.
        decltype(sBanksById){}.swap(sBanksById);
        decltype(sSequencesById){}.swap(sSequencesById);
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
    sPinnedBanks.fill(nullptr);
    sPinnedSequences.fill(nullptr);
    sPinnedSequenceCount = 0;
    sCache.clear();
    sCrcToEntry.clear();
    sBanksById.clear();
    sSequencesById.clear();
    sArchive.reset();
    if (AudioDma_Clear != nullptr) {
        AudioDma_Clear();
    }
}

extern "C" size_t Mk64Resource3DSArchiveEntryCount(void) {
    return sArchive == nullptr ? 0 : sArchive->Entries().size();
}

extern "C" size_t Mk64Resource3DSLoadedCount(void) {
    return sCache.size();
}

extern "C" bool Mk64Resource3DSGetTexture(const char* name, Mk64TextureResource3DS* outTexture) {
    if (outTexture == nullptr) {
        return false;
    }
    *outTexture = {};
    LoadedResource* resource = LoadByPath(NormalizePath(name));
    if (resource == nullptr || resource->type != kTypeTexture || resource->pointer == nullptr ||
        resource->pointerSize == 0 || resource->textureWidth == 0 || resource->textureHeight == 0) {
        return false;
    }
    outTexture->data = static_cast<const uint8_t*>(resource->pointer);
    outTexture->size = resource->pointerSize;
    outTexture->width = resource->textureWidth;
    outTexture->height = resource->textureHeight;
    outTexture->type = resource->textureType;
    return true;
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
    const auto found = sCrcToEntry.find(PathCrc(path));
    if (found == sCrcToEntry.end() || found->second.archiveIndex >= sArchive->Entries().size() ||
        std::string_view(sArchive->Entries()[found->second.archiveIndex]) != path) {
        return false;
    }
    return sArchive->GetEntryUncompressedSizeByIndex(found->second.archiveIndex, byteCount) ==
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
    const auto found = sCrcToEntry.find(crc);
    if (found == sCrcToEntry.end() || found->second.archiveIndex >= sArchive->Entries().size()) {
        return false;
    }
    return sArchive->GetEntryUncompressedSizeByIndex(found->second.archiveIndex, byteCount) ==
           mk64_3ds::O2rReadResult::Ok;
}

extern "C" uint64_t ResourceGetCrcByName(const char* name) {
    const std::string_view path = NormalizePath(name);
    return path.empty() ? 0 : PathCrc(path);
}

extern "C" const char* ResourceGetNameByCrc(uint64_t crc) {
    if (sArchive == nullptr) {
        return nullptr;
    }
    const auto found = sCrcToEntry.find(crc);
    return found == sCrcToEntry.end() ? nullptr : sArchive->Entries()[found->second.archiveIndex].c_str();
}

extern "C" void* ResourceGetDataByName(const char* name) {
    LoadedResource* resource = LoadByPath(NormalizePath(name));
    return resource == nullptr ? nullptr : resource->pointer;
}

extern "C" void* ResourceGetDataByCrc(uint64_t crc) {
    LoadedResource* resource = LoadByCrc(crc);
    return resource == nullptr ? nullptr : resource->pointer;
}

extern "C" size_t ResourceGetSizeByName(const char* name) {
    LoadedResource* resource = LoadByPath(NormalizePath(name));
    return resource == nullptr ? 0 : resource->pointerSize;
}

extern "C" size_t ResourceGetSizeByCrc(uint64_t crc) {
    LoadedResource* resource = LoadByCrc(crc);
    return resource == nullptr ? 0 : resource->pointerSize;
}

extern "C" uint16_t ResourceGetTexWidthByName(const char* name) {
    LoadedResource* resource = LoadByPath(NormalizePath(name));
    return resource == nullptr ? 0 : resource->textureWidth;
}

extern "C" uint16_t ResourceGetTexHeightByName(const char* name) {
    LoadedResource* resource = LoadByPath(NormalizePath(name));
    return resource == nullptr ? 0 : resource->textureHeight;
}

extern "C" size_t ResourceGetTexSizeByName(const char* name) {
    return ResourceGetSizeByName(name);
}

extern "C" uint16_t ResourceGetTexWidthByCrc(uint64_t crc) {
    LoadedResource* resource = LoadByCrc(crc);
    return resource == nullptr ? 0 : resource->textureWidth;
}

extern "C" uint16_t ResourceGetTexHeightByCrc(uint64_t crc) {
    LoadedResource* resource = LoadByCrc(crc);
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
    if (path.rfind("sound/", 0) == 0) {
        return;
    }
    const auto crcEntry = sCrcToEntry.find(PathCrc(path));
    if (crcEntry != sCrcToEntry.end() && sArchive != nullptr &&
        crcEntry->second.archiveIndex < sArchive->Entries().size() &&
        std::string_view(sArchive->Entries()[crcEntry->second.archiveIndex]) == path) {
        const std::string& canonicalPath = sArchive->Entries()[crcEntry->second.archiveIndex];
        const auto loaded = sCache.find(canonicalPath);
        if (loaded == sCache.end() || crcEntry->second.loaded == loaded->second.get()) {
            crcEntry->second.loaded = nullptr;
        }
        if (loaded != sCache.end()) {
            sCache.erase(loaded);
        }
        return;
    }

    // Collision-safe fallback for the exceptional path that was loaded by
    // name because its CRC slot belongs to another archive entry.
    for (auto loaded = sCache.begin(); loaded != sCache.end(); ++loaded) {
        if (std::string_view(loaded->first) == path) {
            sCache.erase(loaded);
            return;
        }
    }
}

extern "C" void ResourceDirtyByCrc(uint64_t crc) {
    const auto found = sCrcToEntry.find(crc);
    if (found != sCrcToEntry.end() && sArchive != nullptr &&
        found->second.archiveIndex < sArchive->Entries().size()) {
        const std::string& path = sArchive->Entries()[found->second.archiveIndex];
        if (path.rfind("sound/", 0) == 0) {
            return;
        }
        found->second.loaded = nullptr;
        sCache.erase(path);
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
extern "C" void ResourceDirtyDirectory(const char*) {
}
extern "C" void ResourceUnloadDirectory(const char*) {
}

extern "C" uint32_t IsResourceManagerLoaded(void) {
    return sArchive != nullptr;
}

extern "C" bool GameEngine_OTRSigCheck(const char* data) {
    return data != nullptr && std::strncmp(data, kOtrSignature.data(), kOtrSignature.size()) == 0;
}

extern "C" int32_t GameEngine_ResourceGetTexTypeByName(const char* name) {
    LoadedResource* resource = LoadByPath(NormalizePath(name));
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
