#include "Companion.h"
#include "install_log_3ds.h"

#include <3ds.h>

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

        // The explicit modding argument is required: without it, C++ prefers
        // the overload that converts sourceDir to bool and silently swaps the
        // source/destination roles on 3DS. Use the path constructor so Torch
        // loads the ROM after parsing config.yml instead of holding 12 MiB
        // during the YAML load.
        instance = Companion::Instance =
            new Companion(std::filesystem::path(rom), ArchiveType::O2R, false, false, sourceDir, destinationDir);
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
        Mk64InstallLogWritef("Torch: memory after exception: %lu free / %lu application bytes.",
                             static_cast<unsigned long>(osGetMemRegionFree(MEMREGION_APPLICATION)),
                             static_cast<unsigned long>(osGetMemRegionSize(MEMREGION_APPLICATION)));
        if (error != nullptr && errorSize != 0) {
            std::snprintf(error, errorSize, "%s", exception.what());
        }
        delete instance;
        Companion::Instance = nullptr;
        return false;
    } catch (...) {
        Mk64InstallLogWrite("Torch: unknown exception.");
        Mk64InstallLogWritef("Torch: memory after exception: %lu free / %lu application bytes.",
                             static_cast<unsigned long>(osGetMemRegionFree(MEMREGION_APPLICATION)),
                             static_cast<unsigned long>(osGetMemRegionSize(MEMREGION_APPLICATION)));
        if (error != nullptr && errorSize != 0) {
            std::snprintf(error, errorSize, "The Torch extractor raised an unknown error.");
        }
        delete instance;
        Companion::Instance = nullptr;
        return false;
    }
}
