#include "resource_runtime_3ds.h"

#include <fast/resource/type/Texture.h>
#include <libultraship/libultra/gbi.h>
#include <libultraship/bridge/resourcebridge.h>
#include <ship/resource/ResourceManager.h>

#include <cstdio>

struct CtlEntry;
struct AudioSequenceData;

extern "C" CtlEntry* GameEngine_LoadBank(uint8_t bankId);
extern "C" AudioSequenceData* GameEngine_LoadSequence(uint8_t sequenceId);
extern "C" uint32_t GameEngine_GetSequenceCount(void);

namespace {
bool ValidateStartupDisplayList(Gfx* commands, size_t commandCount) {
    if (commands == nullptr || commandCount < 3 ||
        static_cast<uint8_t>(commands[0].words.w0 >> 24) != 0x33 ||
        static_cast<uint8_t>(commands[commandCount - 1].words.w0 >> 24) != 0xB8) {
        return false;
    }
    for (size_t i = 0; i < commandCount; ++i) {
        const uint8_t opcode = static_cast<uint8_t>(commands[i].words.w0 >> 24);
        if (opcode != 0x32 && opcode != 0x33) {
            continue;
        }
        if (++i >= commandCount) {
            return false;
        }
        const uint64_t hash = (static_cast<uint64_t>(commands[i].words.w0) << 32) |
                              static_cast<uint32_t>(commands[i].words.w1);
        if (ResourceGetDataByCrc(hash) == nullptr) {
            return false;
        }
    }
    return true;
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <mk64.o2r>\n", argv[0]);
        return 2;
    }
    if (!Mk64Resource3DSInit(argv[1])) {
        std::fprintf(stderr, "failed to initialize resource runtime\n");
        return 1;
    }

    constexpr const char* texture = "textures/common_data/common_texture_item_box_question_mark";
    constexpr const char* vertex = "models/ceremony_data/ceremony_data_seg11_vtx_340";
    constexpr const char* displayList = "models/ceremony_data/silver_trophy_dl";
    constexpr const char* startupDisplayList = "models/startup_logo/dl1";
    constexpr const char* audioSample = "sound/samples/sample_0";
    constexpr uint32_t expectedSequenceCount = 36;
    Ship::ResourceManager resourceManager;
    const auto wrappedTexture =
        std::static_pointer_cast<Fast::Texture>(resourceManager.LoadResourceProcess(texture));
    const uint64_t displayListCrc = ResourceGetCrcByName(displayList);
    auto* startupCommands = static_cast<Gfx*>(ResourceGetDataByName(startupDisplayList));
    const size_t startupCommandCount = ResourceGetSizeByName(startupDisplayList) / sizeof(Gfx);
    const bool startupDisplayListValid = ValidateStartupDisplayList(startupCommands, startupCommandCount);
    const bool ok = wrappedTexture != nullptr && wrappedTexture->ImageData != nullptr &&
                    wrappedTexture->Width == ResourceGetTexWidthByName(texture) &&
                    wrappedTexture->Height == ResourceGetTexHeightByName(texture) &&
                    resourceManager.GetArchiveManager()->HashToCString(displayListCrc) != nullptr &&
                    resourceManager.GetResourceRawPointer(displayListCrc) != nullptr &&
                    ResourceGetDataByName(texture) != nullptr && ResourceGetTexWidthByName(texture) > 0 &&
                    ResourceGetTexHeightByName(texture) > 0 && ResourceGetDataByName(vertex) != nullptr &&
                    ResourceGetSizeByName(vertex) > 0 && ResourceGetDataByName(displayList) != nullptr &&
                    ResourceGetSizeByName(displayList) > 0 && startupDisplayListValid &&
                    ResourceGetDataByName(audioSample) != nullptr &&
                    ResourceGetDataByCrc(displayListCrc) != nullptr &&
                    GameEngine_LoadBank(0) != nullptr && GameEngine_LoadSequence(0) != nullptr &&
                    GameEngine_GetSequenceCount() == expectedSequenceCount;

    std::printf("entries=%zu loaded=%zu result=%s\n", Mk64Resource3DSArchiveEntryCount(),
                Mk64Resource3DSLoadedCount(), ok ? "ok" : "failed");
    Mk64Resource3DSShutdown();
    return ok ? 0 : 1;
}
