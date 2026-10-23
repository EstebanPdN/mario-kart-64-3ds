// Options state, activation, touch and controller actions.
// Included by bottom_ui_3ds.cpp inside its private UI namespace; not a separate TU.

uint8_t RowCount(OptionsTab tab) {
    switch (tab) {
        case OptionsTab::Game: return sUi.modalOpenedFromPause ? 3 : 2;
        case OptionsTab::Screen: return sUi.displayAdvanced ? mk64_3ds::kAdvancedDisplayOptions.size() : mk64_3ds::kDisplayOptions.size();
        case OptionsTab::Gameplay: return 3;
        case OptionsTab::Developer: return 5;
        default: return 1;
    }
}

int VisibleTabCount() { return 4; }
OptionsTab VisibleTab(int index) { return static_cast<OptionsTab>(index); }
int VisibleTabIndex() { return static_cast<int>(sUi.tab); }

float OptionsRowY(OptionsTab tab, uint8_t row) {
    return mk64_3ds::OptionRowY(tab == OptionsTab::Screen, row);
}

void OpenOptions(bool fromPause) {
    sUi.modalOpen = true;
    sUi.modalOpenedFromPause = fromPause;
    sUi.displayAdvanced = false;
    sUi.tab = fromPause ? OptionsTab::Game : OptionsTab::Screen;
    sUi.selectedRow = 0;
    sUi.bottomDirty = true;
}

void CloseOptions() {
    if (sTurboSliderDragging) { sTurboSliderDragging = false; SaveChangedSetting("TURBO SPEED SAVED"); }
    if (sVolumeSliderDragging) {
        sVolumeSliderDragging = false;
        SaveChangedSetting("VOLUME SAVED");
    }
    if (sRenderSliderDragging) {
        sRenderSliderDragging = false;
        SaveChangedSetting("RENDER SCALE SAVED");
    }
    if (Mk64Settings3DSGetOverlayEnabled()) {
        Mk64Settings3DSSetOverlayEnabled(false);
        Mk64Settings3DSSave();
    }
    sUi.modalOpen = false;
    sUi.modalOpenedFromPause = false;
    sUi.selectedRow = 0;
    sUi.bottomDirty = true;
}

void DismissOptions(mk64_3ds::ModalDismissAction3DS action) {
    switch (action) {
        case mk64_3ds::ModalDismissAction3DS::Continue:
            if (Mk64GameState3DSPerformPauseAction(MK64_PAUSE_ACTION_CONTINUE)) {
                sUi.resumeThrottleThisFrame = (sUi.previousHeldKeys & KEY_A) != 0;
                CloseOptions();
            }
            break;
        case mk64_3ds::ModalDismissAction3DS::Close:
            CloseOptions();
            break;
        case mk64_3ds::ModalDismissAction3DS::None:
        default:
            break;
    }
}

void SetTab(OptionsTab tab) {
    if (sTurboSliderDragging) { sTurboSliderDragging = false; SaveChangedSetting("TURBO SPEED SAVED"); }
    if (sVolumeSliderDragging) {
        sVolumeSliderDragging = false;
        SaveChangedSetting("VOLUME SAVED");
    }
    if (sRenderSliderDragging) {
        sRenderSliderDragging = false;
        SaveChangedSetting("RENDER SCALE SAVED");
    }
    sUi.tab = tab;
    sUi.displayAdvanced = false;
    sUi.selectedRow = 0;
    sUi.bottomDirty = true;
}

void ActivateSelectedRow(int direction) {
    const int step = direction < 0 ? -1 : 1;
    switch (sUi.tab) {
        case OptionsTab::Game:
            if ((sUi.modalOpenedFromPause && sUi.selectedRow == 2) ||
                (!sUi.modalOpenedFromPause && sUi.selectedRow == 0)) { OpenUpdate(); return; }
            if (!sUi.modalOpenedFromPause) { CloseOptions(); return; }
            if (Mk64GameState3DSPerformPauseAction(sUi.selectedRow == 0 ? MK64_PAUSE_ACTION_CONTINUE : MK64_PAUSE_ACTION_QUIT)) {
                sUi.resumeThrottleThisFrame = sUi.selectedRow == 0 && (sUi.previousHeldKeys & KEY_A);
                CloseOptions();
            }
            return;
        case OptionsTab::Screen:
            switch (mk64_3ds::DisplayOptionAt(sUi.displayAdvanced, sUi.selectedRow)) {
                case mk64_3ds::DisplayOption::Profile: {
                    const int profile = (Mk64Settings3DSGetDisplayProfile() + step + 3) % 3;
                    Mk64Settings3DSApplyDisplayProfile(static_cast<Mk64DisplayProfile3DS>(profile));
                    SaveChangedSetting("DISPLAY PROFILE SAVED");
                    return;
                }
                case mk64_3ds::DisplayOption::Advanced:
                    sUi.displayAdvanced = true; sUi.selectedRow = 0; sUi.bottomDirty = true; return;
                case mk64_3ds::DisplayOption::Aspect: Mk64Settings3DSSetAspectRatio(Mk64Settings3DSGetAspectRatio() == MK64_ASPECT_RATIO_3DS_WIDE
                    ? MK64_ASPECT_RATIO_3DS_ORIGINAL : MK64_ASPECT_RATIO_3DS_WIDE); break;
                case mk64_3ds::DisplayOption::Hud: Mk64Settings3DSSetHudLayout(static_cast<Mk64HudLayout3DS>((Mk64Settings3DSGetHudLayout() + step + 5) % 5)); break;
                case mk64_3ds::DisplayOption::Render: Mk64Settings3DSSetRenderScalePercent(std::clamp(Mk64Settings3DSGetRenderScalePercent() + step * 5, 50, 100)); break;
                case mk64_3ds::DisplayOption::Distance: Mk64Settings3DSSetRenderDistance(static_cast<Mk64RenderDistance3DS>((Mk64Settings3DSGetRenderDistance() + step + 3) % 3)); break;
                case mk64_3ds::DisplayOption::Filter: Mk64Settings3DSSetDisplayFilter(static_cast<Mk64DisplayFilter3DS>((Mk64Settings3DSGetDisplayFilter() + step + 3) % 3)); break;
                case mk64_3ds::DisplayOption::Loading: Mk64Settings3DSSetShowLoadingScreens(!Mk64Settings3DSGetShowLoadingScreens()); break;
                case mk64_3ds::DisplayOption::FrameLimiter:
                    Mk64Settings3DSSetFrameLimit(static_cast<Mk64FrameLimit3DS>((Mk64Settings3DSGetFrameLimit()+step+3)%3)); break;
                case mk64_3ds::DisplayOption::Meters: Mk64Settings3DSSetHudMeters(static_cast<Mk64HudMeters3DS>((Mk64Settings3DSGetHudMeters() + step + 4) % 4)); break;
                case mk64_3ds::DisplayOption::Blur: Mk64Settings3DSSetMenuBlur((Mk64Settings3DSGetMenuBlur() + step + 4) % 4); break;
                case mk64_3ds::DisplayOption::Maps: Mk64Settings3DSSetMenuMaps(!Mk64Settings3DSGetMenuMaps()); break;
                case mk64_3ds::DisplayOption::Textures:
                    Mk64Settings3DSSetUiTextures(static_cast<Mk64UiTextures3DS>(
                        (Mk64Settings3DSGetUiTextures()+step+3)%3)); break;
                case mk64_3ds::DisplayOption::Warnings: Mk64Settings3DSSetThreatWarnings(!Mk64Settings3DSGetThreatWarnings()); break;
                case mk64_3ds::DisplayOption::Back: sUi.displayAdvanced = false; sUi.selectedRow = mk64_3ds::kAdvancedDisplayRow; sUi.bottomDirty = true; return;
            }
            SaveChangedSetting("DISPLAY SETTINGS SAVED");
            break;
        case OptionsTab::Gameplay:
            if (sUi.selectedRow == 0) {
                Mk64Settings3DSSetTurboMultiplier(std::clamp(Mk64Settings3DSGetTurboMultiplier() + step, 2, 5));
                SaveChangedSetting("TURBO SPEED SAVED");
            } else if (sUi.selectedRow == 1) {
                size_t index = 0;
                for (size_t i=0; i<kVolumeSteps.size(); ++i) if (kVolumeSteps[i] == Mk64Settings3DSGetMasterVolumePercent()) index=i;
                index = direction < 0 ? (index + kVolumeSteps.size()-1) % kVolumeSteps.size() : (index+1) % kVolumeSteps.size();
                Mk64Settings3DSSetMasterVolumePercent(kVolumeSteps[index]);
                SaveChangedSetting("MASTER VOLUME SAVED");
            } else {
                Mk64Settings3DSSetNewItems(!Mk64Settings3DSGetNewItems());
                SaveChangedSetting("MODERN LOGIC: NEXT RACE");
            }
            break;
        case OptionsTab::Developer:
            switch (sUi.selectedRow) {
                case 0: SetStatus(Mk64Diagnostics3DSRequestDump() ? "MEMORY DUMP REQUESTED" : "DUMP ALREADY QUEUED", 2400); break;
                case 1: Mk64Settings3DSSetShowFpsEnabled(!Mk64Settings3DSGetShowFpsEnabled()); SaveChangedSetting("FPS DISPLAY SAVED"); break;
                case 2: Mk64Settings3DSSetSelectAction(Mk64Settings3DSGetSelectAction() == MK64_SELECT_ACTION_3DS_DUMP
                    ? MK64_SELECT_ACTION_3DS_CAMERA : MK64_SELECT_ACTION_3DS_DUMP); SaveChangedSetting("SELECT BINDING SAVED"); break;
                case 3: Mk64Settings3DSSetSelectAction(Mk64Settings3DSGetSelectAction() == MK64_SELECT_ACTION_3DS_OVERLAY
                    ? MK64_SELECT_ACTION_3DS_CAMERA : MK64_SELECT_ACTION_3DS_OVERLAY); SaveChangedSetting("SELECT BINDING SAVED"); break;
                case 4: SetStatus(Mk64Diagnostics3DSRequestCleanDumps() ? "CLEAN DUMPS REQUESTED" : "DUMP ALREADY QUEUED", 2400); break;
            }
            break;
        default: break;
    }
    sUi.bottomDirty = true;
}

bool PointInside(uint16_t x, uint16_t y, int left, int top, int width, int height) {
    return x >= left && x < left + width && y >= top && y < top + height;
}

void HandleOptionsTouch(uint16_t x, uint16_t y) {
    if (y >= kOptionsTabY && y < kOptionsTabY + 24) { SetTab(VisibleTab(std::min<int>(x / 80, 3))); return; }
    for (uint8_t row=0; row<RowCount(sUi.tab); ++row) {
        const int top = OptionsRowY(sUi.tab,row)-3;
        const int height = mk64_3ds::OptionRowHeight(sUi.tab == OptionsTab::Screen);
        if (!PointInside(x,y,12,top,296,height)) continue;
        sUi.selectedRow=row;
        if (sUi.tab == OptionsTab::Screen &&
            mk64_3ds::DisplayOptionAt(sUi.displayAdvanced, row) == mk64_3ds::DisplayOption::Render && x >= 137 && x <= 255) {
            Mk64Settings3DSSetRenderScalePercent(mk64_3ds::RenderScaleFromTouch(x)); sRenderSliderDragging=true;
        } else if (sUi.tab == OptionsTab::Gameplay && row == 1 && x >= 137 && x <= 255) {
            Mk64Settings3DSSetMasterVolumePercent(kVolumeSteps[(std::clamp<int>(x,146,246)-146+10)/20]); sVolumeSliderDragging=true;
        } else if (sUi.tab == OptionsTab::Gameplay && row == 0 && x >= 137 && x <= 255) {
            Mk64Settings3DSSetTurboMultiplier(2+(std::clamp<int>(x,146,246)-146)*3/100); sTurboSliderDragging=true;
        } else ActivateSelectedRow(1);
        sUi.bottomDirty=true; return;
    }
    if (!(sUi.tab == OptionsTab::Screen && sUi.displayAdvanced) && PointInside(x,y,84,207,152,30))
        DismissOptions(mk64_3ds::ResolveModalDismissAction(true,sUi.modalOpenedFromPause));
}

void HandleModalInput(const Mk64DiagnosticsInput3DS& input) {
    if (UpdateIsOpen()) { HandleUpdateInput(input); return; }
    if (sTurboSliderDragging) {
        if (input.touchHeld) { Mk64Settings3DSSetTurboMultiplier(2+(std::clamp<int>(input.touchX,146,246)-146)*3/100); sUi.bottomDirty=true; }
        else { sTurboSliderDragging=false; SaveChangedSetting("TURBO SPEED SAVED"); }
    }
    if (sVolumeSliderDragging) {
        if (input.touchHeld) {
            Mk64Settings3DSSetMasterVolumePercent(kVolumeSteps[
                (std::clamp<int>(input.touchX, 146, 246) - 146 + 10) / 20]);
            sUi.bottomDirty = true;
        } else {
            sVolumeSliderDragging = false;
            SaveChangedSetting("VOLUME SAVED");
        }
    }
    if (sRenderSliderDragging) {
        if (input.touchHeld) {
            Mk64Settings3DSSetRenderScalePercent(mk64_3ds::RenderScaleFromTouch(input.touchX));
            sUi.bottomDirty = true;
        } else {
            sRenderSliderDragging = false;
            SaveChangedSetting("RENDER SCALE SAVED");
        }
    }
    if (sUi.tab == OptionsTab::Screen && sUi.displayAdvanced && (input.downMask & KEY_B)) {
        sUi.displayAdvanced=false; sUi.selectedRow=mk64_3ds::kAdvancedDisplayRow; sUi.bottomDirty=true; return;
    }
    const mk64_3ds::ModalDismissAction3DS dismissAction =
        mk64_3ds::ResolveModalDismissAction(
            (input.downMask & (KEY_B | KEY_START)) != 0,
            sUi.modalOpenedFromPause);
    if (dismissAction != mk64_3ds::ModalDismissAction3DS::None) {
        // Back/Continue is independent of the selected tab and row. Routing it
        // through ActivateSelectedRow used to turn B into MEMORY DUMP whenever
        // Developer row zero happened to be selected.
        DismissOptions(dismissAction);
        return;
    }

    if ((input.downMask & KEY_L) != 0) {
        SetTab(VisibleTab((VisibleTabIndex() + VisibleTabCount() - 1) % VisibleTabCount()));
    } else if ((input.downMask & KEY_R) != 0) {
        SetTab(VisibleTab((VisibleTabIndex() + 1) % VisibleTabCount()));
    }

    const uint8_t rows = RowCount(sUi.tab);
    if ((input.downMask & KEY_DUP) != 0) {
        sUi.selectedRow = static_cast<uint8_t>((sUi.selectedRow + rows - 1U) % rows);
        sUi.bottomDirty = true;
    } else if ((input.downMask & KEY_DDOWN) != 0) {
        sUi.selectedRow = static_cast<uint8_t>((sUi.selectedRow + 1U) % rows);
        sUi.bottomDirty = true;
    }
    if (sUi.tab == OptionsTab::Game) {
        if ((input.downMask & KEY_A) != 0) ActivateSelectedRow(1);
    } else if ((input.downMask & KEY_DLEFT) != 0) {
        ActivateSelectedRow(-1);
    } else if ((input.downMask & (KEY_DRIGHT | KEY_A)) != 0) {
        ActivateSelectedRow(1);
    }
    if ((input.downMask & KEY_TOUCH) != 0) HandleOptionsTouch(input.touchX, input.touchY);
}
