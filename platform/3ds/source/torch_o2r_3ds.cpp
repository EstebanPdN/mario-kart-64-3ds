#include "Companion.h"
#include "install_log_3ds.h"

#include <array>
#include <exception>
#include <filesystem>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

bool Mk64Torch3DSBuildO2R(const char* rom, const char* sourceDir, const char* destinationDir,
                          const char* additionalFile, char* error, size_t errorSize) {
    Companion* instance = nullptr;
    if (error != nullptr && errorSize != 0) error[0] = '\0';

    try {
        Mk64InstallLogWritef("Torch: starting extraction; source=%s destination=%s", sourceDir, destinationDir);
        std::vector<std::string> additionalFiles;
        if (additionalFile != nullptr && additionalFile[0] != '\0') {
            additionalFiles.emplace_back(additionalFile);
        }

        // libstdc++'s filesystem-backed ifstream is unreliable with the 3DS
        // SD filesystem.  Read with stdio here so Torch receives the verified
        // ROM bytes directly instead of reopening the path internally.
        FILE* romFile = std::fopen(rom, "rb");
        if (romFile == nullptr) {
            throw std::runtime_error("Could not open the verified ROM for extraction.");
        }
        std::vector<uint8_t> romData;
        std::array<uint8_t, 64 * 1024> buffer{};
        while (true) {
            const size_t bytesRead = std::fread(buffer.data(), 1, buffer.size(), romFile);
            if (bytesRead != 0) {
                romData.insert(romData.end(), buffer.data(), buffer.data() + bytesRead);
            }
            if (bytesRead != buffer.size()) {
                if (std::ferror(romFile) != 0) {
                    std::fclose(romFile);
                    throw std::runtime_error("Could not read the verified ROM for extraction.");
                }
                break;
            }
        }
        std::fclose(romFile);
        if (romData.empty()) {
            throw std::runtime_error("The verified ROM is empty.");
        }
        Mk64InstallLogWritef("Torch: loaded %zu ROM bytes using stdio.", romData.size());
        Mk64InstallLogWritef("Torch: SHA-1 calculated by Torch: %s", Companion::CalculateHash(romData).c_str());

        // The explicit modding argument is required: without it, C++ prefers
        // the overload that converts sourceDir to bool and silently swaps the
        // source/destination roles on 3DS.
        instance = Companion::Instance =
            new Companion(std::move(romData), ArchiveType::O2R, false, false, sourceDir, destinationDir);
        instance->SetAdditionalFiles(additionalFiles);
        instance->Init(ExportType::Binary);
        const std::string outputPath = instance->GetOutputPath();
        Mk64InstallLogWritef("Torch: reported output path: %s", outputPath.empty() ? "(empty)" : outputPath.c_str());
        FILE* generatedArchive = outputPath.empty() ? nullptr : std::fopen(outputPath.c_str(), "rb");
        if (generatedArchive == nullptr) {
            throw std::runtime_error("Torch completed without creating its O2R archive.");
        }
        std::fclose(generatedArchive);
        delete instance;
        Companion::Instance = nullptr;
        Mk64InstallLogWrite("Torch: extraction and archive finalization completed.");
        return true;
    } catch (const std::exception& exception) {
        Mk64InstallLogWritef("Torch: exception: %s", exception.what());
        if (error != nullptr && errorSize != 0) {
            std::snprintf(error, errorSize, "%s", exception.what());
        }
        delete instance;
        Companion::Instance = nullptr;
        return false;
    } catch (...) {
        Mk64InstallLogWrite("Torch: unknown exception.");
        if (error != nullptr && errorSize != 0) {
            std::snprintf(error, errorSize, "The Torch extractor raised an unknown error.");
        }
        delete instance;
        Companion::Instance = nullptr;
        return false;
    }
}
