#include "settings_3ds.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    const bool newModel = std::getenv("MK64_TEST_NEW_MODEL") != nullptr;
    Mk64Settings3DSSetHardwareModel(newModel);
    Mk64Settings3DSLoad();
    if (argc == 2) {
        if (!std::strcmp(argv[1], "legacy")) {
            assert(Mk64Settings3DSGetDisplayProfile() == MK64_DISPLAY_PROFILE_3DS_CUSTOM);
            assert(Mk64Settings3DSGetHudLayout() == MK64_HUD_LAYOUT_3DS_CLASSIC);
            assert(Mk64Settings3DSGetRenderScalePercent() == 85);
            assert(Mk64Settings3DSGetTurboMultiplier() == 2);
            assert(Mk64Settings3DSGetSelectAction() == MK64_SELECT_ACTION_3DS_CAMERA);
            assert(!Mk64Settings3DSGetOverlayEnabled());
            assert(!Mk64Settings3DSGetZrDebug());
            assert(Mk64Settings3DSGetLogging());
        } else if (!std::strcmp(argv[1], "conflict")) {
            assert(Mk64Settings3DSGetSelectAction() == MK64_SELECT_ACTION_3DS_OVERLAY);
            assert(Mk64Settings3DSSave());
            std::ifstream f("sdmc:/3ds/MK64/mk64-3ds.cfg");
            const std::string cfg((std::istreambuf_iterator<char>(f)), {});
            assert(cfg.find("select_for_dump=off\n") != std::string::npos);
            assert(cfg.find("select_for_overlay=on\n") != std::string::npos);
        } else if (!std::strcmp(argv[1], "reload")) {
            assert(Mk64Settings3DSGetDisplayProfile() == MK64_DISPLAY_PROFILE_3DS_QUALITY);
            assert(Mk64Settings3DSGetSelectAction() == MK64_SELECT_ACTION_3DS_DUMP);
            assert(Mk64Settings3DSGetTurboMultiplier() == 5);
            assert(Mk64Settings3DSGetUiTextures() == MK64_UI_TEXTURES_3DS_ORIGINAL);
            assert(Mk64Settings3DSGetMasterVolumePercent() == 150);
            assert(!Mk64Settings3DSGetNewItems());
            assert(Mk64Settings3DSGetFrameLimiter() == !newModel);
            assert(Mk64Settings3DSGetRenderDistance() == (newModel ? MK64_RENDER_DISTANCE_3DS_HIGH : MK64_RENDER_DISTANCE_3DS_NORMAL));
        } else if (!std::strcmp(argv[1], "scaled")) {
            assert(Mk64Settings3DSGetUiTextures() == MK64_UI_TEXTURES_3DS_SCALED);
            Mk64Settings3DSApplyDisplayProfile(MK64_DISPLAY_PROFILE_3DS_PERFORMANCE);
            assert(Mk64Settings3DSGetUiTextures() == MK64_UI_TEXTURES_3DS_SCALED);
            assert(Mk64Settings3DSSave());
        } else if (!std::strcmp(argv[1], "menu")) {
            assert(Mk64Settings3DSGetUiTextures() == MK64_UI_TEXTURES_3DS_MENU);
            Mk64Settings3DSApplyDisplayProfile(MK64_DISPLAY_PROFILE_3DS_PERFORMANCE);
            assert(Mk64Settings3DSGetUiTextures() == MK64_UI_TEXTURES_3DS_MENU);
            assert(Mk64Settings3DSSave());
        } else if (!std::strcmp(argv[1], "invalid-textures")) {
            assert(Mk64Settings3DSGetUiTextures() == (newModel ? MK64_UI_TEXTURES_3DS_IMPROVED : MK64_UI_TEXTURES_3DS_MENU));
        } else if ((!std::strcmp(argv[1], "frame-off") || !std::strcmp(argv[1], "frame-off-reversed"))) {
            assert(Mk64Settings3DSGetFrameLimit()==MK64_FRAME_LIMIT_3DS_OFF);
            assert(Mk64Settings3DSSave());
        } else if (!std::strcmp(argv[1], "frame-legacy")) {
            assert(Mk64Settings3DSGetFrameLimit()==MK64_FRAME_LIMIT_3DS_60);
        } else if (!std::strcmp(argv[1], "mismatch")) {
            assert(Mk64Settings3DSGetDisplayProfile() == MK64_DISPLAY_PROFILE_3DS_CUSTOM);
            assert(Mk64Settings3DSGetRenderScalePercent() == 75);
        } else assert(false);
        std::puts("configuration migration/reload: passed");
        return 0;
    }

    Mk64Settings3DSResetDefaults();
    assert(Mk64Settings3DSGetHudLayout() == MK64_HUD_LAYOUT_3DS_MKDS_2);
    assert(Mk64Settings3DSGetSelectAction() == MK64_SELECT_ACTION_3DS_CAMERA);
    assert(Mk64Settings3DSGetMenuBlur() == 1 && Mk64Settings3DSGetMenuMaps());
    assert(Mk64Settings3DSGetUiTextures() == (newModel ? MK64_UI_TEXTURES_3DS_IMPROVED : MK64_UI_TEXTURES_3DS_MENU));
    Mk64Settings3DSSetUiTextures(MK64_UI_TEXTURES_3DS_ORIGINAL);
    Mk64Settings3DSSetSelectAction(MK64_SELECT_ACTION_3DS_DUMP);
    Mk64Settings3DSSetTurboMultiplier(5);
    Mk64Settings3DSSetMasterVolumePercent(150);
    Mk64Settings3DSSetNewItems(false);
    for (auto preset : {MK64_DISPLAY_PROFILE_3DS_PERFORMANCE, MK64_DISPLAY_PROFILE_3DS_QUALITY}) {
        Mk64Settings3DSApplyDisplayProfile(preset);
        assert(Mk64Settings3DSGetDisplayProfile() == preset);
        assert(Mk64Settings3DSGetSelectAction() == MK64_SELECT_ACTION_3DS_DUMP);
        assert(Mk64Settings3DSGetTurboMultiplier() == 5 && Mk64Settings3DSGetMasterVolumePercent() == 150);
        assert(!Mk64Settings3DSGetNewItems());
        assert(Mk64Settings3DSGetUiTextures() == MK64_UI_TEXTURES_3DS_ORIGINAL);
        assert(Mk64Settings3DSGetRenderScalePercent() == 100);
        assert(Mk64Settings3DSGetFrameLimiter() == (preset == MK64_DISPLAY_PROFILE_3DS_PERFORMANCE || !newModel));
    }
    Mk64Settings3DSSetHudLayout(MK64_HUD_LAYOUT_3DS_MK7);
    assert(Mk64Settings3DSGetDisplayProfile() == MK64_DISPLAY_PROFILE_3DS_CUSTOM);
    assert(Mk64Settings3DSGetRenderScalePercent() == 100); // Manual changes keep other values.
    Mk64Settings3DSApplyDisplayProfile(MK64_DISPLAY_PROFILE_3DS_QUALITY);
    Mk64Settings3DSSetMenuBlur(3);
    assert(Mk64Settings3DSGetDisplayProfile() == MK64_DISPLAY_PROFILE_3DS_CUSTOM);
    Mk64Settings3DSApplyDisplayProfile(MK64_DISPLAY_PROFILE_3DS_QUALITY);
    assert(Mk64Settings3DSGetMenuBlur() == 1); // Re-selecting restores the actual preset.
    Mk64Settings3DSSetOverlayEnabled(true);
    assert(Mk64Settings3DSSave());
    std::ifstream f("sdmc:/3ds/MK64/mk64-3ds.cfg");
    const std::string cfg((std::istreambuf_iterator<char>(f)), {});
    assert(cfg.find("\noverlay=") == std::string::npos); // Live state never migrates as a modal.
    assert(cfg.find("ui_textures=original\n") != std::string::npos);
    assert(cfg.find("zr_debug=") == std::string::npos);
    std::puts("profiles, manual customization, reapplication, control isolation and persistence: passed");
}
