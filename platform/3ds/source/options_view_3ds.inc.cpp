// Options labels, cursor and drawing using the shared layout policy.
// Included by bottom_ui_3ds.cpp inside its private UI namespace; not a separate TU.

const char* TabName(OptionsTab tab) {
    switch (tab) {
        case OptionsTab::Game: return sUi.modalOpenedFromPause ? "PAUSE" : "GAME";
        case OptionsTab::Screen: return "DISPLAY";
        case OptionsTab::Gameplay: return "GAMEPLAY";
        case OptionsTab::Developer: return "DEVELOPER";
        default: return "";
    }
}

bool OptionValueUnavailable(OptionsTab tab, uint8_t row) {
    return tab == OptionsTab::Screen &&
        mk64_3ds::DisplayOptionAt(sUi.displayAdvanced, row) == mk64_3ds::DisplayOption::Meters &&
        !mk64_3ds::HudMetersAvailable(Mk64Settings3DSGetHudLayout(), Mk64Settings3DSGetHudMeters());
}

void GetRowText(OptionsTab tab, uint8_t row, const char** label, char* value, size_t valueSize) {
    *label=""; value[0]=0;
    auto put=[&](const char* text) { std::snprintf(value,valueSize,"%s",text); };
    switch (tab) {
        case OptionsTab::Game:
            *label = sUi.modalOpenedFromPause ? (row == 0 ? "CONTINUE GAME" : row == 1 ? "QUIT" : "UPDATE") : (row == 0 ? "UPDATE" : "CLOSE"); break;
        case OptionsTab::Screen:
            switch (mk64_3ds::DisplayOptionAt(sUi.displayAdvanced, row)) {
                case mk64_3ds::DisplayOption::Profile: {
                    *label="PROFILE"; constexpr const char* names[]={"PERFORMANCE","QUALITY","CUSTOM"};
                    put(names[Mk64Settings3DSGetDisplayProfile()]); break;
                }
                case mk64_3ds::DisplayOption::Advanced: *label="ADVANCED"; put(">"); break;
                case mk64_3ds::DisplayOption::Aspect: *label="ASPECT RATIO"; put(Mk64Settings3DSGetAspectRatio() == MK64_ASPECT_RATIO_3DS_WIDE ? "WIDE" : "ORIGINAL 4:3"); break;
                case mk64_3ds::DisplayOption::Hud: { *label="HUD LAYOUT"; constexpr const char* names[]={"CLEAN","MK7","MKDS","MKDS 2","CLASSIC"}; put(names[Mk64Settings3DSGetHudLayout()]); break; }
                case mk64_3ds::DisplayOption::Render: *label="RENDER"; std::snprintf(value,valueSize,"%u%%",Mk64Settings3DSGetRenderScalePercent()); break;
                case mk64_3ds::DisplayOption::Distance: { *label="DISTANCE"; constexpr const char* names[]={"LOW","NORMAL","HIGH"}; put(names[Mk64Settings3DSGetRenderDistance()]); break; }
                case mk64_3ds::DisplayOption::Filter: { *label="FILTER"; constexpr const char* names[]={"NORMAL","BLUR","CRT"}; put(names[Mk64Settings3DSGetDisplayFilter()]); break; }
                case mk64_3ds::DisplayOption::Loading: *label="LOADING SCREENS"; put(Mk64Settings3DSGetShowLoadingScreens() ? "ON" : "OFF"); break;
                case mk64_3ds::DisplayOption::FrameLimiter: *label="FRAME LIMITER";
                    put(Mk64Settings3DSGetFrameLimit()==MK64_FRAME_LIMIT_3DS_30 ? "30 FPS" :
                        (Mk64Settings3DSGetFrameLimit()==MK64_FRAME_LIMIT_3DS_60 ? "60 FPS" : "OFF")); break;
                case mk64_3ds::DisplayOption::Meters: { *label="HUD METERS"; constexpr const char* names[]={"OFF","DISTANCE","SPEED","POSITION/LAP"}; put(names[Mk64Settings3DSGetHudMeters()]); break; }
                case mk64_3ds::DisplayOption::Blur: { *label="MENU BLUR"; constexpr const char* names[]={"OFF","LIGHT","MEDIUM","STRONG"}; put(names[Mk64Settings3DSGetMenuBlur()]); break; }
                case mk64_3ds::DisplayOption::Maps: *label="SCROLLING MAPS"; put(Mk64Settings3DSGetMenuMaps() ? "ON" : "OFF"); break;
                case mk64_3ds::DisplayOption::Textures: *label="TEXTURE";
                    switch (Mk64Settings3DSGetUiTextures()) {
                        case MK64_UI_TEXTURES_3DS_ORIGINAL: put("ORIGINAL"); break;
                        case MK64_UI_TEXTURES_3DS_MENU: put("MENU"); break;
                        default: put("IMPROVED"); break;
                    }
                    break;
                case mk64_3ds::DisplayOption::Warnings: *label="WARNING WIDGET"; put(Mk64Settings3DSGetThreatWarnings() ? "ON" : "OFF"); break;
                case mk64_3ds::DisplayOption::Back: *label="BACK TO DISPLAY"; break;
            }
            break;
        case OptionsTab::Gameplay:
            if (row == 0) { *label="TURBO SPEED"; std::snprintf(value,valueSize,"%uX",Mk64Settings3DSGetTurboMultiplier()); }
            else if (row == 1) { *label="VOLUME"; std::snprintf(value,valueSize,"%u%%",Mk64Settings3DSGetMasterVolumePercent()); }
            else { *label="MODERN LOGIC"; put(Mk64Settings3DSGetNewItems() ? "ON" : "OFF"); }
            break;
        case OptionsTab::Developer:
            switch (row) {
                case 0: *label="MEMORY DUMP"; put("CREATE"); break;
                case 1: *label="SHOW FPS"; put(Mk64Settings3DSGetShowFpsEnabled() ? "ON" : "OFF"); break;
                case 2: *label="SELECT FOR DUMP"; put(Mk64Settings3DSGetSelectAction() == MK64_SELECT_ACTION_3DS_DUMP ? "ON" : "OFF"); break;
                case 3: *label="SELECT FOR OVERLAY"; put(Mk64Settings3DSGetSelectAction() == MK64_SELECT_ACTION_3DS_OVERLAY ? "ON" : "OFF"); break;
                case 4: *label="CLEAN DUMPS"; put("CLEAN"); break;
            }
            break;
        default: break;
    }
}

mk64_3ds::OptionIcon OptionIconForRow(OptionsTab tab, uint8_t row) {
    using mk64_3ds::OptionIcon;
    switch (tab) {
        case OptionsTab::Screen:
            if (mk64_3ds::DisplayOptionAt(sUi.displayAdvanced, row) == mk64_3ds::DisplayOption::Textures) return OptionIcon::Render;
            if (mk64_3ds::DisplayOptionAt(sUi.displayAdvanced, row) == mk64_3ds::DisplayOption::Render) return OptionIcon::Dump;
            return static_cast<OptionIcon>(mk64_3ds::DisplayOptionAt(sUi.displayAdvanced, row));
        case OptionsTab::Gameplay:
            return static_cast<OptionIcon>(static_cast<unsigned>(OptionIcon::Turbo) + row);
        case OptionsTab::Developer:
            return static_cast<OptionIcon>(static_cast<unsigned>(OptionIcon::Dump) + row);
        case OptionsTab::Game:
            return sUi.modalOpenedFromPause
                ? static_cast<OptionIcon>(static_cast<unsigned>(OptionIcon::Continue) + row)
                : (row == 0 ? OptionIcon::Update : OptionIcon::Back);
        default: return OptionIcon::Advanced;
    }
}

void DrawOptionIcon(mk64_3ds::OptionIcon icon, float x, float y) {
    if (!sUi.optionIcons.initialized) return;
    const auto index = static_cast<unsigned>(icon);
    DrawTextureRegion(sUi.optionIcons, (index % 7) * 18, (index / 7) * 18,
                      18, 18, x, y, 18, 18, .73f);
}

void DrawOptions() {
    if (sUi.game.racing) DrawRaceBackground(205.0f); else DrawDimMenuBackground();
    C2D_DrawRectSolid(0.0f, 0.0f, 0.25f, kBottomWidth, kBottomHeight,
                      C2D_Color32(0, 0, 0, 82));
    for (int index = 0; index < VisibleTabCount(); ++index) {
        const auto tab = VisibleTab(index);
        const bool selected = tab == sUi.tab;
        const float center = (index + 0.5f) * kBottomWidth / VisibleTabCount();
        DrawText(TabName(tab), center,
                 kOptionsTabY + 4.0f, tab == OptionsTab::Developer ? 0.5f : 0.58f,
                 selected ? C2D_Color32(255, 225, 75, 255)
                          : C2D_Color32(210, 220, 220, 230),
                 C2D_AlignCenter, 0.72f);
        if (selected) C2D_DrawRectSolid(center - 18, kOptionsTabY + 16, .73f,
            36, 2, C2D_Color32(255, 225, 75, 255));

    }
    const uint8_t rows = RowCount(sUi.tab);
    for (uint8_t row = 0; row < rows; ++row) {
        const float y = OptionsRowY(sUi.tab, row);
        const char* label = "";
        char value[48] = {};
        GetRowText(sUi.tab, row, &label, value, sizeof(value));
        const bool selected = row == sUi.selectedRow;
        const uint8_t shade = selected ? mk64_3ds::OptionSelectionShade(sUi.game.menuFlashTick) : 1;
        C2D_DrawRectSolid(12, y - 3, .69f, 296, 18, C2D_Color32(shade, shade, shade, 255));
        // The native recess has a dark perimeter and a muted lower/right edge.
        C2D_DrawRectSolid(307, y - 2, .70f, 1, 17, C2D_Color32(106,106,106,255));
        C2D_DrawRectSolid(30, y + 14, .70f, 278, 1, C2D_Color32(106,106,106,255));
        DrawOptionIcon(OptionIconForRow(sUi.tab, row), 12, y - 3);
        sDataFontPalette = sUi.dataFonts[5].initialized ? 5 : -1;
        constexpr float labelScale = .62f;
        DrawText(label, 34.0f, y+1, labelScale,
                 selected ? C2D_Color32(255, 255, 255, 255)
                          : C2D_Color32(238, 238, 230, 245),
                 C2D_AlignLeft, 0.74f);
        DrawText(value, 304.0f, y+1, .56f,
                 OptionValueUnavailable(sUi.tab, row) ? C2D_Color32(255, 90, 90, 255)
                     : (selected ? C2D_Color32(255, 255, 255, 255)
                                 : C2D_Color32(198, 222, 210, 240)),
                 C2D_AlignRight, 0.74f);
        sDataFontPalette = -1;
        const float sliderCenter = mk64_3ds::OptionRowCenterY(sUi.tab == OptionsTab::Screen, row);
        if ((sUi.tab == OptionsTab::Screen &&
            mk64_3ds::DisplayOptionAt(sUi.displayAdvanced, row) == mk64_3ds::DisplayOption::Render) ||
            (sUi.tab == OptionsTab::Gameplay && row <= 1)) {
            // Original racing-style rail: yellow lane, step markers and a
            // red/white chequered grip. The row's flashing plate remains
            // visible between these pieces; the rail has no opaque backing.
            C2D_DrawRectSolid(146, sliderCenter - 1, 0.76f, 100, 2, C2D_Color32(255, 219, 66, 255));
            const bool volume = sUi.tab == OptionsTab::Gameplay && row == 1;
            const bool turbo = sUi.tab == OptionsTab::Gameplay && row == 0;
            const int ticks = volume ? 5 : turbo ? 3 : 10;
            for (int tick = 0; tick <= ticks; ++tick)
                C2D_DrawRectSolid(146 + tick * 100.0f / ticks, sliderCenter - 3, 0.77f, 1, 6, C2D_Color32(235, 235, 215, 255));
            int position = (Mk64Settings3DSGetRenderScalePercent() - 50) * 2;
            if (volume) {
                for (size_t i = 0; i < kVolumeSteps.size(); ++i)
                    if (kVolumeSteps[i] == Mk64Settings3DSGetMasterVolumePercent()) position = i * 20;
            }
            if (turbo) position = (Mk64Settings3DSGetTurboMultiplier()-2)*100/3;
            const float grip = 146 + position;
            C2D_DrawRectSolid(grip - 4, sliderCenter - 6, 0.78f, 9, 12, C2D_Color32(235, 45, 35, 255));
            for (int cell = 0; cell < 6; ++cell)
                if ((cell / 2 + cell % 2) % 2 == 0)
                    C2D_DrawRectSolid(grip - 3 + (cell % 2) * 4, sliderCenter - 5 + (cell / 2) * 3,
                                      0.79f, 3, 3, C2D_Color32(255, 250, 220, 255));
        }

    }
    if (!sUi.modalOpenedFromPause && !(sUi.tab == OptionsTab::Screen && sUi.displayAdvanced)) DrawText("B BACK",
        160.0f, 214.0f, 0.68f, C2D_Color32(235, 238, 225, 255),C2D_AlignCenter,0.74f);


}
