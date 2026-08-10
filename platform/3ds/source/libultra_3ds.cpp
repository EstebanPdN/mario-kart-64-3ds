#define MK64_3DS_LIBULTRA_ONLY
#include <libultraship.h>

#include "audio_ndsp_3ds.h"
#include "input_3ds.h"
#include "system_3ds.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

extern "C" size_t AudioDma_Clamp(uintptr_t address, size_t bytes);

namespace {

constexpr uint64_t kN64ClockRate = 62500000ULL;
constexpr size_t kEepromSize = EEPROM_MAXBLOCKS * EEPROM_BLOCK_SIZE;
constexpr size_t kVirtualPakCapacity = 512 * 1024;
constexpr const char* kSaveDirectory = "sdmc:/3ds/MK64";
constexpr const char* kEepromPath = "sdmc:/3ds/MK64/eeprom.bin";
constexpr const char* kEepromTempPath = "sdmc:/3ds/MK64/eeprom.tmp";
constexpr const char* kVirtualPakPath = "sdmc:/3ds/MK64/controller-pak.bin";
constexpr const char* kVirtualPakTempPath = "sdmc:/3ds/MK64/controller-pak.tmp";
constexpr const char* kLegacyEepromPath = "sdmc:/3ds/spaghettikart/eeprom.bin";
constexpr const char* kLegacyVirtualPakPath = "sdmc:/3ds/spaghettikart/controller-pak.bin";

struct VirtualPakHeader {
    char magic[8];
    uint32_t version;
    OSPfsState state;
    uint32_t dataSize;
};

uint64_t sTimeOffset = 0;
uint32_t sAudioRate = 32000;
uint8_t sEeprom[kEepromSize] = {};
bool sEepromLoaded = false;
bool sVirtualPakLoaded = false;
bool sVirtualPakExists = false;
OSPfsState sVirtualPakState = {};
std::vector<uint8_t> sVirtualPakData;

uint64_t RawN64Time() {
    const uint64_t ticks = Mk64System3DSGetTick();
    const uint64_t tickRate = Mk64System3DSTicksPerSecond();
    const uint64_t seconds = ticks / tickRate;
    const uint64_t remainder = ticks % tickRate;
    return seconds * kN64ClockRate + remainder * kN64ClockRate / tickRate;
}

bool FinishAtomicWrite(FILE* file, const char* temporaryPath, const char* destinationPath, bool ok) {
    if (file == nullptr) {
        return false;
    }
    if (ok && std::fflush(file) != 0) {
        ok = false;
    }
    if (ok && fsync(fileno(file)) != 0) {
        ok = false;
    }
    if (std::fclose(file) != 0) {
        ok = false;
    }
    if (!ok || rename(temporaryPath, destinationPath) != 0) {
        std::remove(temporaryPath);
        return false;
    }
    return true;
}

void LoadEeprom() {
    if (sEepromLoaded) {
        return;
    }
    sEepromLoaded = true;
    std::memset(sEeprom, 0xFF, sizeof(sEeprom));
    FILE* file = std::fopen(kEepromPath, "rb");
    if (file == nullptr) {
        // v0.14 and older accidentally used the SpaghettiKart directory and a
        // 2 KiB EEPROM image. Accept the first 512 bytes so an existing save is
        // migrated to the correct location on its next write.
        file = std::fopen(kLegacyEepromPath, "rb");
    }
    if (file != nullptr) {
        uint8_t loaded[kEepromSize] = {};
        if (std::fread(loaded, 1, sizeof(loaded), file) == sizeof(loaded)) {
            std::memcpy(sEeprom, loaded, sizeof(sEeprom));
        }
        std::fclose(file);
    }
}

bool StoreEeprom() {
    mkdir("sdmc:/3ds", 0777);
    mkdir(kSaveDirectory, 0777);
    FILE* file = std::fopen(kEepromTempPath, "wb");
    if (file == nullptr) {
        return false;
    }
    const bool ok = std::fwrite(sEeprom, 1, sizeof(sEeprom), file) == sizeof(sEeprom);
    return FinishAtomicWrite(file, kEepromTempPath, kEepromPath, ok);
}

void LoadVirtualPak() {
    if (sVirtualPakLoaded) {
        return;
    }
    sVirtualPakLoaded = true;
    FILE* file = std::fopen(kVirtualPakPath, "rb");
    if (file == nullptr) {
        file = std::fopen(kLegacyVirtualPakPath, "rb");
    }
    if (file == nullptr) {
        return;
    }
    VirtualPakHeader header = {};
    const bool validHeader = std::fread(&header, 1, sizeof(header), file) == sizeof(header) &&
                             std::memcmp(header.magic, "MK643DSP", sizeof(header.magic)) == 0 &&
                             header.version == 1 && header.dataSize <= kVirtualPakCapacity;
    if (validHeader) {
        sVirtualPakData.resize(header.dataSize);
        if (header.dataSize == 0 ||
            std::fread(sVirtualPakData.data(), 1, header.dataSize, file) == header.dataSize) {
            sVirtualPakState = header.state;
            sVirtualPakExists = true;
        } else {
            sVirtualPakData.clear();
        }
    }
    std::fclose(file);
}

bool StoreVirtualPak() {
    mkdir("sdmc:/3ds", 0777);
    mkdir(kSaveDirectory, 0777);
    FILE* file = std::fopen(kVirtualPakTempPath, "wb");
    if (file == nullptr) {
        return false;
    }
    VirtualPakHeader header = {};
    std::memcpy(header.magic, "MK643DSP", sizeof(header.magic));
    header.version = 1;
    header.state = sVirtualPakState;
    header.dataSize = static_cast<uint32_t>(sVirtualPakData.size());
    const bool ok = std::fwrite(&header, 1, sizeof(header), file) == sizeof(header) &&
                    (sVirtualPakData.empty() ||
                     std::fwrite(sVirtualPakData.data(), 1, sVirtualPakData.size(), file) == sVirtualPakData.size());
    return FinishAtomicWrite(file, kVirtualPakTempPath, kVirtualPakPath, ok);
}

bool VirtualPakNameMatches(u16 companyCode, u32 gameCode, const u8* gameName, const u8* extName) {
    return sVirtualPakExists && sVirtualPakState.company_code == companyCode && sVirtualPakState.game_code == gameCode &&
           gameName != nullptr && extName != nullptr &&
           std::memcmp(sVirtualPakState.game_name, gameName, PFS_FILE_NAME_LEN) == 0 &&
           std::memcmp(sVirtualPakState.ext_name, extName, PFS_FILE_EXT_LEN) == 0;
}

} // namespace

extern "C" {

u64 osClockRate = kN64ClockRate;
u32 osTvType = OS_TV_NTSC;
u32 osResetType = 0;
u8 osAppNmiBuffer[64] = {};
u8 __osMaxControllers = MAXCONTROLLERS;

void osInitialize(void) {
    Mk64Input3DSInit();
}

int32_t osContInit(OSMesgQueue*, uint8_t* controllerBits, OSContStatus* status) {
    if (controllerBits != nullptr) {
        *controllerBits = 1;
    }
    if (status != nullptr) {
        std::memset(status, 0, sizeof(OSContStatus) * MAXCONTROLLERS);
        status[0].type = CONT_TYPE_NORMAL;
        status[0].status = CONT_CARD_ON;
    }
    return 0;
}

int32_t osContStartReadData(OSMesgQueue* queue) {
    if (queue != nullptr) {
        OSMesg message = {};
        osSendMesg(queue, message, OS_MESG_NOBLOCK);
    }
    return 0;
}

void osContGetReadData(OSContPad* pads) {
    if (pads == nullptr) {
        return;
    }
    std::memset(pads, 0, sizeof(OSContPad) * MAXCONTROLLERS);
    Mk64Pad3DS source = {};
    Mk64Input3DSPoll(&source);
    pads[0].button = source.buttons;
    pads[0].stick_x = source.stickX;
    pads[0].stick_y = source.stickY;
    pads[0].right_stick_x = source.rightStickX;
    pads[0].right_stick_y = source.rightStickY;
}

uint8_t osContGetStatus(uint8_t controller) {
    return controller == 0 ? 0 : CONT_NO_RESPONSE_ERROR;
}

void osCreateMesgQueue(OSMesgQueue* queue, OSMesg* buffer, int32_t count) {
    if (queue == nullptr) {
        return;
    }
    queue->mtqueue = nullptr;
    queue->fullqueue = nullptr;
    queue->validCount = 0;
    queue->first = 0;
    queue->msgCount = count;
    queue->msg = buffer;
}

int32_t osSendMesg(OSMesgQueue* queue, OSMesg message, int32_t) {
    if (queue == nullptr || queue->msg == nullptr || queue->msgCount <= 0 || queue->validCount >= queue->msgCount) {
        return -1;
    }
    const int32_t index = (queue->first + queue->validCount) % queue->msgCount;
    queue->msg[index] = message;
    ++queue->validCount;
    return 0;
}

int32_t osJamMesg(OSMesgQueue* queue, OSMesg message, int32_t) {
    if (queue == nullptr || queue->msg == nullptr || queue->msgCount <= 0 || queue->validCount >= queue->msgCount) {
        return -1;
    }
    queue->first = (queue->first + queue->msgCount - 1) % queue->msgCount;
    queue->msg[queue->first] = message;
    ++queue->validCount;
    return 0;
}

int32_t osRecvMesg(OSMesgQueue* queue, OSMesg* message, int32_t) {
    if (queue == nullptr || queue->msg == nullptr || queue->validCount <= 0) {
        return -1;
    }
    if (message != nullptr) {
        *message = queue->msg[queue->first];
    }
    queue->first = (queue->first + 1) % queue->msgCount;
    --queue->validCount;
    return 0;
}

void osSetEventMesg(OSEvent, OSMesgQueue*, OSMesg) {
}

void osSetTime(OSTime time) {
    sTimeOffset = time - RawN64Time();
}

uint32_t osGetCount(void) {
    return static_cast<uint32_t>(RawN64Time() + sTimeOffset);
}

s32 osPfsIsPlug(OSMesgQueue*, u8* pattern) {
    if (pattern != nullptr) {
        *pattern = 1;
    }
    return 0;
}

s32 osPfsInit(OSMesgQueue* queue, OSPfs* pfs, int channel) {
    if (pfs == nullptr || channel != 0) {
        return PFS_ERR_NOPACK;
    }
    LoadVirtualPak();
    std::memset(pfs, 0, sizeof(*pfs));
    pfs->status = PFS_INITIALIZED;
    pfs->queue = queue;
    pfs->channel = channel;
    pfs->version = OS_PFS_VERSION;
    pfs->banks = 1;
    return 0;
}

s32 osPfsNumFiles(OSPfs* pfs, s32* maxFiles, s32* filesUsed) {
    if (pfs == nullptr || pfs->channel != 0 || maxFiles == nullptr || filesUsed == nullptr) {
        return PFS_ERR_INVALID;
    }
    LoadVirtualPak();
    *maxFiles = 16;
    *filesUsed = sVirtualPakExists ? 1 : 0;
    return 0;
}

s32 osPfsFreeBlocks(OSPfs* pfs, s32* bytesNotUsed) {
    if (pfs == nullptr || pfs->channel != 0 || bytesNotUsed == nullptr) {
        return PFS_ERR_INVALID;
    }
    LoadVirtualPak();
    *bytesNotUsed = static_cast<s32>(kVirtualPakCapacity - sVirtualPakData.size());
    return 0;
}

s32 osPfsFindFile(OSPfs* pfs, u16 companyCode, u32 gameCode, u8* gameName, u8* extName, s32* fileNo) {
    if (pfs == nullptr || pfs->channel != 0 || fileNo == nullptr) {
        return PFS_ERR_INVALID;
    }
    LoadVirtualPak();
    if (!VirtualPakNameMatches(companyCode, gameCode, gameName, extName)) {
        return PFS_ERR_INVALID;
    }
    *fileNo = 0;
    return 0;
}

s32 osPfsAllocateFile(OSPfs* pfs, u16 companyCode, u32 gameCode, u8* gameName, u8* extName,
                      int fileSizeInBytes, s32* fileNo) {
    if (pfs == nullptr || pfs->channel != 0 || gameName == nullptr || extName == nullptr || fileNo == nullptr ||
        fileSizeInBytes < 0 || static_cast<size_t>(fileSizeInBytes) > kVirtualPakCapacity) {
        return PFS_ERR_INVALID;
    }
    LoadVirtualPak();
    if (sVirtualPakExists) {
        return PFS_ERR_EXIST;
    }
    std::memset(&sVirtualPakState, 0, sizeof(sVirtualPakState));
    sVirtualPakState.company_code = companyCode;
    sVirtualPakState.game_code = gameCode;
    sVirtualPakState.file_size = static_cast<u32>(fileSizeInBytes);
    std::memcpy(sVirtualPakState.game_name, gameName, PFS_FILE_NAME_LEN);
    std::memcpy(sVirtualPakState.ext_name, extName, PFS_FILE_EXT_LEN);
    sVirtualPakData.assign(static_cast<size_t>(fileSizeInBytes), 0);
    sVirtualPakExists = true;
    *fileNo = 0;
    return StoreVirtualPak() ? 0 : PFS_ERR_CONTRFAIL;
}

s32 osPfsReadWriteFile(OSPfs* pfs, s32 fileNo, u8 flag, int offset, int sizeInBytes, u8* dataBuffer) {
    if (pfs == nullptr || pfs->channel != 0 || fileNo != 0 || offset < 0 || sizeInBytes < 0 || dataBuffer == nullptr) {
        return PFS_ERR_INVALID;
    }
    LoadVirtualPak();
    if (!sVirtualPakExists) {
        return PFS_ERR_INVALID;
    }
    const size_t begin = static_cast<size_t>(offset);
    const size_t end = begin + static_cast<size_t>(sizeInBytes);
    if (end < begin || end > kVirtualPakCapacity) {
        return PFS_ERR_INVALID;
    }
    if (flag == PFS_READ) {
        if (end > sVirtualPakData.size()) {
            return PFS_ERR_BAD_DATA;
        }
        std::memcpy(dataBuffer, sVirtualPakData.data() + begin, static_cast<size_t>(sizeInBytes));
        return 0;
    }
    if (flag != PFS_WRITE) {
        return PFS_ERR_INVALID;
    }
    if (end > sVirtualPakData.size()) {
        sVirtualPakData.resize(end, 0);
    }
    std::memcpy(sVirtualPakData.data() + begin, dataBuffer, static_cast<size_t>(sizeInBytes));
    sVirtualPakState.file_size = static_cast<u32>(sVirtualPakData.size());
    return StoreVirtualPak() ? 0 : PFS_ERR_CONTRFAIL;
}

s32 osPfsFileState(OSPfs* pfs, s32 fileNo, OSPfsState* state) {
    if (pfs == nullptr || pfs->channel != 0 || fileNo != 0 || state == nullptr) {
        return PFS_ERR_INVALID;
    }
    LoadVirtualPak();
    if (!sVirtualPakExists) {
        return PFS_ERR_INVALID;
    }
    *state = sVirtualPakState;
    return 0;
}

s32 osPfsDeleteFile(OSPfs* pfs, u16 companyCode, u32 gameCode, u8* gameName, u8* extName) {
    if (pfs == nullptr || pfs->channel != 0) {
        return PFS_ERR_INVALID;
    }
    LoadVirtualPak();
    if (!VirtualPakNameMatches(companyCode, gameCode, gameName, extName)) {
        return PFS_ERR_INVALID;
    }
    sVirtualPakExists = false;
    sVirtualPakData.clear();
    std::memset(&sVirtualPakState, 0, sizeof(sVirtualPakState));
    return std::remove(kVirtualPakPath) == 0 ? 0 : PFS_ERR_CONTRFAIL;
}

s32 osAiSetFrequency(u32 frequency) {
    sAudioRate = frequency;
    return static_cast<s32>(frequency);
}

u32 osAiGetLength(void) {
#if defined(MK64_3DS_ENABLE_NDSP_AUDIO)
    return Mk64Audio3DSBufferedFrames() * sizeof(int16_t) * 2;
#else
    return 0;
#endif
}

s32 osAiSetNextBuffer(void* buffer, size_t bytes) {
    if (buffer == nullptr || bytes == 0 || (bytes % (sizeof(int16_t) * 2)) != 0) {
        return -1;
    }
#if defined(MK64_3DS_ENABLE_NDSP_AUDIO)
    if (!Mk64Audio3DSInit(sAudioRate)) {
        return -1;
    }
    const size_t frames = bytes / (sizeof(int16_t) * 2);
    return Mk64Audio3DSQueueStereoS16(static_cast<const int16_t*>(buffer), frames) ? 0 : -1;
#else
    (void)buffer;
    return 0;
#endif
}

s32 osPiStartDma(OSIoMesg*, s32, s32, uintptr_t source, void* destination, size_t bytes, OSMesgQueue* queue) {
    if (source == 0 || destination == nullptr) {
        return -1;
    }
    size_t safeBytes = bytes;
    safeBytes = AudioDma_Clamp(source, bytes);
    if (safeBytes < bytes) {
        std::memset(destination, 0, bytes);
    }
    std::memcpy(destination, reinterpret_cast<const void*>(source), safeBytes);
    if (queue != nullptr) {
        OSMesg message = {};
        osSendMesg(queue, message, OS_MESG_NOBLOCK);
    }
    return 0;
}

s32 osEepromProbe(OSMesgQueue*) {
    LoadEeprom();
    return EEPROM_TYPE_4K;
}

s32 osEepromLongRead(OSMesgQueue*, u8 address, u8* buffer, int nbytes) {
    LoadEeprom();
    const size_t offset = static_cast<size_t>(address) * EEPROM_BLOCK_SIZE;
    if (buffer == nullptr || nbytes < 0) {
        return -1;
    }
    const size_t bytes = static_cast<size_t>(nbytes);
    if (offset > sizeof(sEeprom) || bytes > sizeof(sEeprom) - offset) {
        return -1;
    }
    std::memcpy(buffer, sEeprom + offset, bytes);
    return 0;
}

s32 osEepromLongWrite(OSMesgQueue*, u8 address, u8* buffer, int nbytes) {
    LoadEeprom();
    const size_t offset = static_cast<size_t>(address) * EEPROM_BLOCK_SIZE;
    if (buffer == nullptr || nbytes < 0) {
        return -1;
    }
    const size_t bytes = static_cast<size_t>(nbytes);
    if (offset > sizeof(sEeprom) || bytes > sizeof(sEeprom) - offset) {
        return -1;
    }
    std::memcpy(sEeprom + offset, buffer, bytes);
    return StoreEeprom() ? 0 : -1;
}

void osWritebackDCacheAll(void) {
}
void osInvalDCache(void*, int32_t) {
}
void osInvalICache(void*, int32_t) {
}
void osWritebackDCache(void*, int32_t) {
}

void osCreatePiManager(OSPri, OSMesgQueue*, OSMesg*, s32) {
}
void osCreateViManager(OSPri) {
}
void osViSetEvent(OSMesgQueue*, OSMesg, u32) {
}
void osViSwapBuffer(void*) {
}
void osViBlack(uint8_t) {
}
void osViSetSpecialFeatures(u32) {
}
void osViSetMode(OSViMode*) {
}

void osCreateThread(OSThread* thread, OSId id, void (*)(void*), void*, void*, OSPri priority) {
    if (thread != nullptr) {
        thread->id = id;
        thread->priority = priority;
        thread->state = OS_STATE_STOPPED;
    }
}
void osStartThread(OSThread* thread) {
    if (thread != nullptr) {
        thread->state = OS_STATE_RUNNING;
    }
}
void osSetThreadPri(OSThread* thread, OSPri priority) {
    if (thread != nullptr) {
        thread->priority = priority;
    }
}

void osSpTaskLoad(OSTask*) {
}
void osSpTaskStartGo(OSTask*) {
}
void osSpTaskYield(void) {
}
OSYieldResult osSpTaskYielded(OSTask*) {
    return 0;
}

void create_debug_thread(void) {
}
void start_debug_thread(void) {
}
void func_80040174(void*, s32, s32) {
}
void mio0decode(u8*, u8*) {
}
s32 mio0encode(s32, s32, s32) {
    return -1;
}
void rmonPrintf(const char*, ...) {
}
void lusprintf(const char*, int32_t, int32_t, const char*, ...) {
}

} // extern "C"
