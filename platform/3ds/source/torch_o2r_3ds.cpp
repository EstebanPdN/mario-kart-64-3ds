#include "Companion.h"

#include <exception>
#include <filesystem>
#include <string>
#include <vector>

bool Mk64Torch3DSBuildO2R(const char* rom, const char* sourceDir, const char* destinationDir,
                          const char* additionalFile) {
    Companion* instance = nullptr;

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
    } catch (const std::exception&) {
        delete instance;
        Companion::Instance = nullptr;
        return false;
    } catch (...) {
        delete instance;
        Companion::Instance = nullptr;
        return false;
    }
}
