#include "game_data_3ds.h"

#include <3ds.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace {
constexpr const char* kDataDir = "sdmc:/3ds/MK64";
constexpr const char* kPrimaryArchivePath = "sdmc:/3ds/MK64/mk64.o2r";
constexpr const char* kLegacyArchivePath = "sdmc:/3ds/mk64-3ds/mk64.o2r";
constexpr const char* kExpectedSha1 = "579c48e211ae952530ffc8738709f078d5dd215e";
constexpr std::array<const char*, 3> kRomPaths = {
    "sdmc:/3ds/MK64/Mario Kart 64.z64",
    "sdmc:/3ds/MK64/mk64.z64",
    "sdmc:/3ds/MK64/baserom.us.z64",
};

struct Sha1Context {
    uint32_t state[5];
    uint64_t bitCount;
    uint8_t buffer[64];
};

bool FileExists(const char* path) {
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }
    std::fclose(file);
    return true;
}

void SetResult(Mk64GameData3DSResult* result, Mk64GameData3DSStatus status, const char* archivePath,
               const char* message) {
    result->status = status;
    result->archivePath = archivePath;
    std::snprintf(result->message, sizeof(result->message), "%s", message);
}

void DrawProgress(const char* status, const char* detail, int percent) {
    consoleClear();
    std::printf("Mario Kart 64 3DS\n\n");
    std::printf("Preparing game data\n");
    std::printf("-------------------\n\n");
    std::printf("%s\n\n", status);
    if (detail != nullptr && detail[0] != '\0') {
        std::printf("%s\n\n", detail);
    }
    std::printf("[");
    const int filled = percent / 5;
    for (int i = 0; i < 20; ++i) {
        std::printf("%c", i < filled ? '#' : ' ');
    }
    std::printf("] %d%%\n\n", percent);
    std::printf("Folder:\n/3ds/MK64/\n");
    gspWaitForVBlank();
}

uint32_t Rol32(uint32_t value, uint32_t bits) {
    return (value << bits) | (value >> (32U - bits));
}

void Sha1Transform(Sha1Context* ctx, const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i) {
        w[i] = Rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];

    for (int i = 0; i < 80; ++i) {
        uint32_t f = 0;
        uint32_t k = 0;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        const uint32_t temp = Rol32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = Rol32(b, 30);
        b = a;
        a = temp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

void Sha1Init(Sha1Context* ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->bitCount = 0;
    std::memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void Sha1Update(Sha1Context* ctx, const uint8_t* data, size_t len) {
    size_t bufferIndex = static_cast<size_t>((ctx->bitCount / 8) % 64);
    ctx->bitCount += static_cast<uint64_t>(len) * 8U;

    size_t offset = 0;
    if (bufferIndex != 0) {
        const size_t needed = 64 - bufferIndex;
        if (len < needed) {
            std::memcpy(ctx->buffer + bufferIndex, data, len);
            return;
        }
        std::memcpy(ctx->buffer + bufferIndex, data, needed);
        Sha1Transform(ctx, ctx->buffer);
        offset += needed;
    }

    while (offset + 64 <= len) {
        Sha1Transform(ctx, data + offset);
        offset += 64;
    }

    if (offset < len) {
        std::memcpy(ctx->buffer, data + offset, len - offset);
    }
}

void Sha1Final(Sha1Context* ctx, uint8_t digest[20]) {
    uint8_t padding[64] = {0x80};
    uint8_t length[8];
    for (int i = 0; i < 8; ++i) {
        length[7 - i] = static_cast<uint8_t>((ctx->bitCount >> (i * 8)) & 0xFF);
    }

    const size_t bufferIndex = static_cast<size_t>((ctx->bitCount / 8) % 64);
    const size_t padLength = bufferIndex < 56 ? (56 - bufferIndex) : (120 - bufferIndex);
    Sha1Update(ctx, padding, padLength);
    Sha1Update(ctx, length, sizeof(length));

    for (int i = 0; i < 5; ++i) {
        digest[i * 4] = static_cast<uint8_t>((ctx->state[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = static_cast<uint8_t>((ctx->state[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = static_cast<uint8_t>((ctx->state[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = static_cast<uint8_t>(ctx->state[i] & 0xFF);
    }
}

bool Sha1File(const char* path, char outHex[41]) {
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    Sha1Context ctx;
    Sha1Init(&ctx);
    std::array<uint8_t, 64 * 1024> buffer{};
    long processed = 0;
    int lastPercent = -1;

    while (!std::feof(file)) {
        const size_t read = std::fread(buffer.data(), 1, buffer.size(), file);
        if (read == 0) {
            break;
        }
        Sha1Update(&ctx, buffer.data(), read);
        processed += static_cast<long>(read);
        const int percent = size > 0 ? static_cast<int>((processed * 70L) / size) + 10 : 50;
        if (percent != lastPercent) {
            DrawProgress("Checking ROM...", path, percent);
            lastPercent = percent;
        }
    }
    std::fclose(file);

    uint8_t digest[20];
    Sha1Final(&ctx, digest);
    for (int i = 0; i < 20; ++i) {
        std::snprintf(outHex + i * 2, 3, "%02x", digest[i]);
    }
    outHex[40] = '\0';
    return true;
}

const char* FindRom() {
    for (const char* path : kRomPaths) {
        if (FileExists(path)) {
            return path;
        }
    }
    return nullptr;
}
} // namespace

extern "C" Mk64GameData3DSResult Mk64GameData3DSEnsure(void) {
    Mk64GameData3DSResult result{};

    if (mkdir("sdmc:/3ds", 0777) != 0 && errno != EEXIST) {
        SetResult(&result, MK64_GAME_DATA_ERROR, nullptr, "Could not create sd:/3ds.");
        return result;
    }
    if (mkdir(kDataDir, 0777) != 0 && errno != EEXIST) {
        SetResult(&result, MK64_GAME_DATA_ERROR, nullptr, "Could not create sd:/3ds/MK64.");
        return result;
    }

    if (FileExists(kPrimaryArchivePath)) {
        SetResult(&result, MK64_GAME_DATA_READY, kPrimaryArchivePath, "Game data is ready.");
        return result;
    }
    if (FileExists(kLegacyArchivePath)) {
        SetResult(&result, MK64_GAME_DATA_READY, kLegacyArchivePath, "Game data is ready.");
        return result;
    }

    gfxInitDefault();
    consoleInit(GFX_TOP, nullptr);
    DrawProgress("Looking for your ROM...", "Put it in /3ds/MK64/", 5);

    const char* romPath = FindRom();
    if (romPath == nullptr) {
        gfxExit();
        SetResult(&result, MK64_GAME_DATA_MISSING_ROM, nullptr,
                  "Place your legally owned Mario Kart 64 USA ROM in sd:/3ds/MK64/.");
        return result;
    }

    char sha1[41];
    if (!Sha1File(romPath, sha1)) {
        gfxExit();
        SetResult(&result, MK64_GAME_DATA_ERROR, nullptr, "The ROM could not be read from sd:/3ds/MK64/.");
        return result;
    }

    if (std::strcmp(sha1, kExpectedSha1) != 0) {
        gfxExit();
        SetResult(&result, MK64_GAME_DATA_BAD_ROM, nullptr,
                  "The ROM in sd:/3ds/MK64/ is not the supported Mario Kart 64 USA dump.");
        return result;
    }

    DrawProgress("ROM verified.", "The 3DS extractor still needs the Torch asset pipeline port.", 85);
    svcSleepThread(1200LL * 1000LL * 1000LL);
    gfxExit();

    SetResult(&result, MK64_GAME_DATA_EXTRACTOR_PENDING, nullptr,
              "ROM verified in sd:/3ds/MK64/. On-device O2R generation is not complete in this build.");
    return result;
}
