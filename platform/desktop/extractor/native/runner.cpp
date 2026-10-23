#include "Companion.h"
#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>

void Mk64DesktopProgress(const char* path) {
    const auto extension = std::filesystem::path(path).extension();
    if (extension != ".yml" && extension != ".yaml") return;
    std::printf("MK64_PROGRESS\t%s\n", path);
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "Usage: mk64-extract ROM METADATA OUTPUT_DIRECTORY\n");
        return 2;
    }
    try {
        // Python passes private ASCII relative paths. This also supports a
        // Unicode installation / user folder on Windows without narrow argv.
        auto engine = std::make_unique<Companion>(std::filesystem::path(argv[1]),
            ArchiveType::O2R, false, false, argv[2], argv[3]);
        Companion::Instance = engine.get();
        engine->SetAdditionalFiles({"meta/mods.toml"});
        engine->Init(ExportType::Binary);
        Companion::Instance = nullptr;
        auto output = std::filesystem::path(argv[3]) / "mk64.o2r";
        if (!std::filesystem::is_regular_file(output)) {
            throw std::runtime_error("Extractor produced no archive.");
        }
        return 0;
    } catch (const std::exception& error) {
        Companion::Instance = nullptr;
        std::fprintf(stderr, "Extraction failed: %s\n", error.what());
        return 1;
    }
}
