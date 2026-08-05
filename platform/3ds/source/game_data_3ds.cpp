#include "game_data_3ds.h"
#include "install_log_3ds.h"

#include <3ds.h>

#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(MK64_3DS_ON_DEVICE_EXTRACTOR)
bool Mk64Torch3DSBuildO2R(const char* rom, const char* sourceDir, const char* destinationDir,
                          const char* additionalFile, char* error, size_t errorSize);
#endif

namespace {
constexpr const char* kDataDir = "sdmc:/3ds/MK64";
constexpr const char* kPrimaryArchivePath = "sdmc:/3ds/MK64/mk64.o2r";
constexpr const char* kLegacyArchivePath = "sdmc:/3ds/mk64-3ds/mk64.o2r";
constexpr const char* kInstallerDir = "sdmc:/3ds/MK64/.mk64-3ds-installer";
constexpr const char* kExtractorSourceDir = "sdmc:/3ds/MK64/.mk64-3ds-installer/torch";
constexpr const char* kExtractorWorkDir = "sdmc:/3ds/MK64/.mk64-3ds-installer/work";
constexpr const char* kWorkArchivePath = "sdmc:/3ds/MK64/.mk64-3ds-installer/work/mk64.o2r";
constexpr const char* kExtractorAdditionalFile = "meta/mods.toml";
constexpr const char* kRomfsExtractorSourceDir = "romfs:/torch";
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

struct CopyStats {
    uint32_t fileCount = 0;
    uint64_t byteCount = 0;
};

PrintConsole gBottomConsole{};
std::array<std::array<char, 76>, 22> gInstallConsoleLines{};
uint32_t gInstallConsoleLineCount = 0;
int gInstallProgressPercent = 0;

void PushInstallConsoleLine(const char* format, ...) {
    char line[76] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    if (gInstallConsoleLineCount < gInstallConsoleLines.size()) {
        std::snprintf(gInstallConsoleLines[gInstallConsoleLineCount++].data(), gInstallConsoleLines[0].size(), "%s", line);
        return;
    }

    for (size_t i = 1; i < gInstallConsoleLines.size(); ++i) {
        gInstallConsoleLines[i - 1] = gInstallConsoleLines[i];
    }
    std::snprintf(gInstallConsoleLines.back().data(), gInstallConsoleLines[0].size(), "%s", line);
}

void DrawTopPixel(uint8_t* frameBuffer, uint16_t frameBufferHeight, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (frameBuffer == nullptr || x < 0 || x >= 400 || y < 0 || y >= 240) {
        return;
    }

    const size_t offset = static_cast<size_t>(x) * frameBufferHeight * 3 + static_cast<size_t>(y) * 3;
    frameBuffer[offset + 0] = b;
    frameBuffer[offset + 1] = g;
    frameBuffer[offset + 2] = r;
}

void DrawTopRect(uint8_t* frameBuffer, uint16_t frameBufferHeight, int x, int y, int width, int height, uint8_t r,
                 uint8_t g, uint8_t b) {
    for (int py = y; py < y + height; ++py) {
        for (int px = x; px < x + width; ++px) {
            DrawTopPixel(frameBuffer, frameBufferHeight, px, py, r, g, b);
        }
    }
}

void DrawLoadingTopScreen(int percent) {
    uint16_t frameBufferWidth = 0;
    uint16_t frameBufferHeight = 0;
    uint8_t* frameBuffer = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &frameBufferWidth, &frameBufferHeight);
    (void) frameBufferWidth;
    if (frameBuffer == nullptr) {
        return;
    }

    DrawTopRect(frameBuffer, frameBufferHeight, 0, 0, 400, 240, 5, 8, 16);
    DrawTopRect(frameBuffer, frameBufferHeight, 0, 0, 400, 26, 16, 31, 54);
    DrawTopRect(frameBuffer, frameBufferHeight, 0, 26, 400, 4, 230, 56, 62);
    DrawTopRect(frameBuffer, frameBufferHeight, 0, 30, 400, 3, 245, 245, 245);
    DrawTopRect(frameBuffer, frameBufferHeight, 0, 33, 400, 4, 48, 126, 211);
    DrawTopRect(frameBuffer, frameBufferHeight, 26, 58, 348, 98, 12, 20, 36);
    DrawTopRect(frameBuffer, frameBufferHeight, 26, 58, 348, 2, 255, 202, 52);
    DrawTopRect(frameBuffer, frameBufferHeight, 26, 154, 348, 2, 255, 202, 52);
    DrawTopRect(frameBuffer, frameBufferHeight, 30, 62, 4, 90, 255, 202, 52);
    DrawTopRect(frameBuffer, frameBufferHeight, 366, 62, 4, 90, 255, 202, 52);

    const int clampedPercent = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
    const int barX = 48;
    const int barY = 205;
    const int barW = 304;
    const int barH = 12;
    DrawTopRect(frameBuffer, frameBufferHeight, barX - 2, barY - 2, barW + 4, barH + 4, 8, 12, 22);
    DrawTopRect(frameBuffer, frameBufferHeight, barX, barY, barW, barH, 38, 44, 58);
    DrawTopRect(frameBuffer, frameBufferHeight, barX, barY, (barW * clampedPercent) / 100, barH, 255, 202, 52);
    DrawTopRect(frameBuffer, frameBufferHeight, barX, barY, barW, 2, 245, 248, 255);
}

void RedrawInstallScreens(int percent) {
    gInstallProgressPercent = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
    DrawLoadingTopScreen(gInstallProgressPercent);
    consoleSelect(&gBottomConsole);
    consoleClear();
    std::printf("Mario Kart 64 3DS\n\n");
    std::printf("Installer output\n");
    std::printf("----------------\n");
    for (uint32_t i = 0; i < gInstallConsoleLineCount; ++i) {
        std::printf("%s\n", gInstallConsoleLines[i].data());
    }
    std::printf("\nLog: /3ds/MK64/mk64-install.log\n");
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

void OnInstallLogLine(const char* message) {
    PushInstallConsoleLine("%s", message != nullptr ? message : "");

    int entries = 0;
    if (message != nullptr && std::sscanf(message, "O2R progress: %d entries", &entries) == 1) {
        const int estimatedPercent = 82 + entries / 1200;
        RedrawInstallScreens(estimatedPercent > 98 ? 98 : estimatedPercent);
        return;
    }
    RedrawInstallScreens(gInstallProgressPercent);
}

bool FileExists(const char* path) {
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }
    std::fclose(file);
    return true;
}

bool DirectoryExists(const char* path) {
    struct stat info {};
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

bool MakeDirectory(const char* path) {
    return mkdir(path, 0777) == 0 || errno == EEXIST;
}

bool JoinPath(char* output, size_t outputSize, const char* base, const char* name) {
    const int written = std::snprintf(output, outputSize, "%s/%s", base, name);
    return written > 0 && static_cast<size_t>(written) < outputSize;
}

bool CopyFile(const char* source, const char* destination, CopyStats* stats) {
    FILE* input = std::fopen(source, "rb");
    if (input == nullptr) return false;
    FILE* output = std::fopen(destination, "wb");
    if (output == nullptr) {
        std::fclose(input);
        return false;
    }

    std::array<uint8_t, 64 * 1024> buffer{};
    bool ok = true;
    while (!std::feof(input)) {
        const size_t bytesRead = std::fread(buffer.data(), 1, buffer.size(), input);
        if (bytesRead == 0) break;
        if (std::fwrite(buffer.data(), 1, bytesRead, output) != bytesRead) {
            ok = false;
            break;
        }
        stats->byteCount += bytesRead;
    }
    if (std::ferror(input) != 0) ok = false;
    std::fclose(output);
    std::fclose(input);
    if (ok) ++stats->fileCount;
    return ok;
}

bool CopyDirectoryTree(const char* source, const char* destination, CopyStats* stats) {
    if (!MakeDirectory(destination)) return false;
    DIR* directory = opendir(source);
    if (directory == nullptr) return false;

    bool ok = true;
    while (ok) {
        dirent* entry = readdir(directory);
        if (entry == nullptr) break;
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;

        char sourcePath[768];
        char destinationPath[768];
        if (!JoinPath(sourcePath, sizeof(sourcePath), source, entry->d_name) ||
            !JoinPath(destinationPath, sizeof(destinationPath), destination, entry->d_name)) {
            ok = false;
            break;
        }

        struct stat info {};
        if (stat(sourcePath, &info) != 0) {
            ok = false;
        } else if (S_ISDIR(info.st_mode)) {
            ok = CopyDirectoryTree(sourcePath, destinationPath, stats);
        } else if (S_ISREG(info.st_mode)) {
            ok = CopyFile(sourcePath, destinationPath, stats);
        }
    }
    closedir(directory);
    return ok;
}

void SetResult(Mk64GameData3DSResult* result, Mk64GameData3DSStatus status, const char* archivePath,
               const char* message) {
    result->status = status;
    result->archivePath = archivePath;
    std::snprintf(result->message, sizeof(result->message), "%s", message);
}

void DrawProgress(const char* status, const char* detail, int percent) {
    PushInstallConsoleLine("%3d%% %s", percent, status != nullptr ? status : "");
    if (detail != nullptr && detail[0] != '\0') {
        PushInstallConsoleLine("     %s", detail);
    }

    RedrawInstallScreens(percent);
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
    const bool readOk = std::ferror(file) == 0;
    std::fclose(file);
    if (!readOk) return false;

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

bool InstallExtractorFiles(CopyStats* stats) {
    if (DirectoryExists(kExtractorSourceDir) && FileExists("sdmc:/3ds/MK64/.mk64-3ds-installer/torch/config.yml")) {
        Mk64InstallLogWrite("Extractor metadata already present on SD card.");
        return true;
    }

    if (!MakeDirectory(kInstallerDir)) return false;
    DrawProgress("Installing local extractor...", "Copying the bundled extractor files to the SD card.", 78);
    Mk64InstallLogWrite("Copying bundled Torch metadata from RomFS to SD card.");
    return CopyDirectoryTree(kRomfsExtractorSourceDir, kExtractorSourceDir, stats);
}

bool GenerateArchiveFromRom(const char* romPath, char* error, size_t errorSize) {
#if defined(MK64_3DS_ON_DEVICE_EXTRACTOR)
    DrawProgress("ROM verified.", "Preparing the local extractor on the SD card.", 76);
    Result romfsResult = romfsInit();
    Mk64InstallLogWritef("RomFS initialization returned 0x%08lX.", static_cast<unsigned long>(romfsResult));
    if (R_FAILED(romfsResult)) {
        std::snprintf(error, errorSize, "RomFS could not be opened from this install.");
        DrawProgress("Extractor metadata unavailable.", error, 76);
        svcSleepThread(1800LL * 1000LL * 1000LL);
        return false;
    }

    CopyStats copyStats{};
    const bool installed = InstallExtractorFiles(&copyStats);
    romfsExit();
    if (!installed) {
        Mk64InstallLogWritef("Extractor metadata copy failed after %lu files and %llu bytes; errno=%d.",
                             static_cast<unsigned long>(copyStats.fileCount),
                             static_cast<unsigned long long>(copyStats.byteCount), errno);
        std::snprintf(error, errorSize, "Could not copy the extractor files to /3ds/MK64/.");
        DrawProgress("Extractor install failed.", error, 78);
        svcSleepThread(1800LL * 1000LL * 1000LL);
        return false;
    }
    Mk64InstallLogWritef("Extractor metadata ready: %lu files, %llu bytes.",
                         static_cast<unsigned long>(copyStats.fileCount),
                         static_cast<unsigned long long>(copyStats.byteCount));

    if (!MakeDirectory(kExtractorWorkDir)) {
        Mk64InstallLogWritef("Could not create temporary extraction directory; errno=%d.", errno);
        std::snprintf(error, errorSize, "Could not create the temporary extraction folder.");
        return false;
    }
    const int removeResult = unlink(kWorkArchivePath);
    if (removeResult == 0) {
        Mk64InstallLogWrite("Removed a previous temporary O2R archive.");
    } else if (errno != ENOENT) {
        Mk64InstallLogWritef("Could not remove previous temporary O2R archive; errno=%d.", errno);
    }

    DrawProgress("Generating mk64.o2r...", "Writing the archive directly to the SD card. This can take several minutes.", 82);
    Mk64InstallLogWrite("Starting O2R archive generation.");
    char originalDirectory[768] = {};
    if (getcwd(originalDirectory, sizeof(originalDirectory)) == nullptr) {
        Mk64InstallLogWritef("Could not read the current working directory; errno=%d.", errno);
        std::snprintf(error, errorSize, "Could not prepare the local extractor directory.");
        return false;
    }
    Mk64InstallLogWritef("Installer working directory before Torch: %s", originalDirectory);

    const char* romName = std::strrchr(romPath, '/');
    romName = romName == nullptr ? romPath : romName + 1;
    char relativeRomPath[192] = {};
    if (std::snprintf(relativeRomPath, sizeof(relativeRomPath), "../../%s", romName) <= 0 ||
        std::strlen(relativeRomPath) >= sizeof(relativeRomPath)) {
        Mk64InstallLogWrite("Could not build a relative ROM path for Torch.");
        std::snprintf(error, errorSize, "Could not prepare the ROM path for extraction.");
        return false;
    }

    if (chdir(kExtractorSourceDir) != 0) {
        Mk64InstallLogWritef("Could not enter the local Torch directory; errno=%d.", errno);
        std::snprintf(error, errorSize, "Could not open the local extractor directory.");
        return false;
    }
    char extractorDirectory[768] = {};
    if (getcwd(extractorDirectory, sizeof(extractorDirectory)) != nullptr) {
        Mk64InstallLogWritef("Installer working directory for Torch: %s", extractorDirectory);
    }
    Mk64InstallLogWrite("Torch uses relative paths to avoid 3DS filesystem handling of sdmc: paths.");
    bool ok = false;
    try {
        ok = Mk64Torch3DSBuildO2R(relativeRomPath, ".", "../work", kExtractorAdditionalFile, error, errorSize);
    } catch (...) {
        std::snprintf(error, errorSize, "The extractor stopped unexpectedly.");
        ok = false;
    }
    if (chdir(originalDirectory) != 0) {
        Mk64InstallLogWritef("Could not restore the working directory; errno=%d.", errno);
        if (ok) {
            std::snprintf(error, errorSize, "The extractor could not restore its working directory.");
            ok = false;
        }
    }

    if (ok && FileExists(kWorkArchivePath) && rename(kWorkArchivePath, kPrimaryArchivePath) == 0) {
        Mk64InstallLogWrite("Temporary O2R archive finalized and moved into /3ds/MK64/mk64.o2r.");
        DrawProgress("Game data generated.", "mk64.o2r was created in /3ds/MK64/.", 100);
        svcSleepThread(900LL * 1000LL * 1000LL);
        return true;
    }

    Mk64InstallLogWritef("O2R generation did not complete; Torch success=%d, error=%s.", ok ? 1 : 0,
                         error[0] != '\0' ? error : "(no extractor error)");
    if (ok) {
        Mk64InstallLogWritef("Could not move finalized O2R archive into place; errno=%d.", errno);
    }
    unlink(kWorkArchivePath);
    if (error[0] == '\0') {
        std::snprintf(error, errorSize, "The temporary O2R archive could not be finalized on the SD card.");
    }
    DrawProgress("O2R generation failed.", error, 82);
    svcSleepThread(2500LL * 1000LL * 1000LL);
    return false;
#else
    (void)romPath;
    std::snprintf(error, errorSize, "This build was made without the on-device extractor.");
    DrawProgress("Extractor unavailable.", "This build was made without the on-device Torch pipeline.", 82);
    svcSleepThread(1800LL * 1000LL * 1000LL);
    return false;
#endif
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

    Mk64InstallLogBegin();
    Mk64InstallLogWrite("Installation check started.");

    if (FileExists(kPrimaryArchivePath)) {
        Mk64InstallLogWrite("Existing O2R archive found; no installation work is needed.");
        Mk64InstallLogClose();
        SetResult(&result, MK64_GAME_DATA_READY, kPrimaryArchivePath, "Game data is ready.");
        return result;
    }
    if (FileExists(kLegacyArchivePath)) {
        Mk64InstallLogWrite("Existing legacy O2R archive found; no installation work is needed.");
        Mk64InstallLogClose();
        SetResult(&result, MK64_GAME_DATA_READY, kLegacyArchivePath, "Game data is ready.");
        return result;
    }

    gfxInitDefault();
    consoleInit(GFX_BOTTOM, &gBottomConsole);
    gInstallConsoleLineCount = 0;
    gInstallProgressPercent = 0;
    Mk64InstallLogSetCallback(OnInstallLogLine);
    DrawProgress("Looking for your ROM...", "Put it in /3ds/MK64/", 5);

    const char* romPath = FindRom();
    if (romPath == nullptr) {
        Mk64InstallLogWrite("No supported ROM filename was found in /3ds/MK64/.");
        Mk64InstallLogSetCallback(nullptr);
        gfxExit();
        Mk64InstallLogClose();
        SetResult(&result, MK64_GAME_DATA_MISSING_ROM, nullptr,
                  "Place your legally owned Mario Kart 64 USA ROM in sd:/3ds/MK64/.");
        return result;
    }

    char sha1[41];
    Mk64InstallLogWritef("ROM found at %s; validating SHA-1.", romPath);
    if (!Sha1File(romPath, sha1)) {
        Mk64InstallLogWritef("ROM validation read failed; errno=%d.", errno);
        Mk64InstallLogSetCallback(nullptr);
        gfxExit();
        Mk64InstallLogClose();
        SetResult(&result, MK64_GAME_DATA_ERROR, nullptr, "The ROM could not be read from sd:/3ds/MK64/.");
        return result;
    }

    if (std::strcmp(sha1, kExpectedSha1) != 0) {
        Mk64InstallLogWrite("ROM SHA-1 does not match the supported Mario Kart 64 USA revision.");
        Mk64InstallLogSetCallback(nullptr);
        gfxExit();
        Mk64InstallLogClose();
        SetResult(&result, MK64_GAME_DATA_BAD_ROM, nullptr,
                  "The ROM in sd:/3ds/MK64/ is not the supported Mario Kart 64 USA dump.");
        return result;
    }
    Mk64InstallLogWrite("ROM SHA-1 verified.");

    char extractionError[384] = {};
    if (!GenerateArchiveFromRom(romPath, extractionError, sizeof(extractionError))) {
        Mk64InstallLogWritef("Installation failed: %s", extractionError);
        Mk64InstallLogSetCallback(nullptr);
        gfxExit();
        Mk64InstallLogClose();
        SetResult(&result, MK64_GAME_DATA_ERROR, nullptr, extractionError);
        return result;
    }
    Mk64InstallLogWrite("Installation completed successfully.");
    Mk64InstallLogSetCallback(nullptr);
    gfxExit();
    Mk64InstallLogClose();

    SetResult(&result, MK64_GAME_DATA_READY, kPrimaryArchivePath, "Game data is ready.");
    return result;
}
