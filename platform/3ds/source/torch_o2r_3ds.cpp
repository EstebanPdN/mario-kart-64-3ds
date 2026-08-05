#include "Companion.h"

#include <exception>
#include <filesystem>
#include <cstdio>
#include <string>
#include <vector>

bool Mk64Torch3DSBuildO2R(const char* rom, const char* sourceDir, const char* destinationDir,
                          const char* additionalFile, char* error, size_t errorSize) {
    Companion* instance = nullptr;
    if (error != nullptr && errorSize != 0) error[0] = '\0';

    try {
        std::vector<std::string> additionalFiles;
        if (additionalFile != nullptr && additionalFile[0] != '\0') {
            additionalFiles.emplace_back(additionalFile);
        }

        instance = Companion::Instance =
            new Companion(std::filesystem::path(rom), ArchiveType::O2R, false, sourceDir, destinationDir);
        instance->SetAdditionalFiles(additionalFiles);
        instance->Init(ExportType::Binary);
        delete instance;
        Companion::Instance = nullptr;
        return true;
    } catch (const std::exception& exception) {
        if (error != nullptr && errorSize != 0) {
            std::snprintf(error, errorSize, "%s", exception.what());
        }
        delete instance;
        Companion::Instance = nullptr;
        return false;
    } catch (...) {
        if (error != nullptr && errorSize != 0) {
            std::snprintf(error, errorSize, "The Torch extractor raised an unknown error.");
        }
        delete instance;
        Companion::Instance = nullptr;
        return false;
    }
}
