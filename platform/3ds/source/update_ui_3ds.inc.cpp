// Updater presentation uses the same native font atlas as Options.
// Included by bottom_ui_3ds.cpp to share its renderer and texture lifetime.
struct UpdateUiState {
    bool open = false;
    bool confirm = false;
    bool confirmYes = false;
    unsigned row = 0;
    unsigned page = 0;
    unsigned lineCount = 0;
    unsigned notesRevision = UINT32_MAX;
    UpdateStatus status = {};
    char notes[12289] = {};
    char lines[384][43] = {};
};
UpdateUiState sUpdate;
constexpr unsigned kUpdateLinesPerPage = 11;

void RefreshUpdateStatus() {
    UpdateStatus latest = {};
    Updater_GetStatus(&latest);
    if (latest.revision != sUpdate.status.revision) sUi.bottomDirty = true;
    sUpdate.status = latest;
}

void OpenUpdate() {
    sUpdate.open = true;
    sUpdate.confirm = false;
    sUpdate.confirmYes = false;
    sUpdate.row = 0;
    sUpdate.page = 0;
    sUpdate.notesRevision = UINT32_MAX;
    sUpdate.notes[0] = 0;
    sUi.bottomDirty = true;
    Updater_Check();
    RefreshUpdateStatus();
}

void CloseUpdate() {
    sUpdate.open = false;
    sUpdate.confirm = false;
    sUi.selectedRow = 1;
    sUi.bottomDirty = true;
}

void UpdateAction(unsigned row) {
    if (Updater_Busy()) return;
    switch (row) {
        case 0:
            sUpdate.page = 0;
            Updater_SetChannel(!sUpdate.status.prerelease);
            break;
        case 1: Updater_Check(); break;
        case 2:
            if (sUpdate.status.state == UPDATE_AVAILABLE) {
                sUpdate.confirm = true;
                sUpdate.confirmYes = false;
            }
            break;
        case 3: CloseUpdate(); break;
    }
    sUi.bottomDirty = true;
}

void HandleUpdateInput(const Mk64DiagnosticsInput3DS& input) {
    RefreshUpdateStatus();
    if ((input.downMask & KEY_L) && sUpdate.page) --sUpdate.page;
    if ((input.downMask & KEY_R) && (sUpdate.page + 1) * kUpdateLinesPerPage < sUpdate.lineCount)
        ++sUpdate.page;
    const bool back = (input.downMask & (KEY_B | KEY_START)) != 0;
    const bool touch = (input.downMask & KEY_TOUCH) != 0;
    if (Updater_Busy()) {
        if (sUpdate.status.state != UPDATE_INSTALLING &&
            (back || (touch && PointInside(input.touchX, input.touchY, 56, 202, 208, 35))))
            Updater_Cancel();
        return;
    }
    if (sUpdate.confirm) {
        if (back) sUpdate.confirm = false;
        else {
            if (input.downMask & (KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN))
                sUpdate.confirmYes = !sUpdate.confirmYes;
            bool activate = (input.downMask & KEY_A) != 0;
            if (touch && input.touchY >= 157 && input.touchY < 197) {
                if (input.touchX >= 48 && input.touchX < 144) { sUpdate.confirmYes = true; activate = true; }
                else if (input.touchX >= 176 && input.touchX < 272) { sUpdate.confirmYes = false; activate = true; }
            }
            if (activate) {
                sUpdate.confirm = false;
                if (sUpdate.confirmYes) Updater_Download();
            }
        }
    } else if (back) CloseUpdate();
    else {
        if (input.downMask & KEY_DUP) sUpdate.row = (sUpdate.row + 3) % 4;
        else if (input.downMask & KEY_DDOWN) sUpdate.row = (sUpdate.row + 1) % 4;
        if ((input.downMask & (KEY_DLEFT | KEY_DRIGHT)) && sUpdate.row == 0) UpdateAction(0);
        else if (input.downMask & KEY_A) UpdateAction(sUpdate.row);
        if (touch) {
            for (unsigned row = 0; row < 4; ++row) {
                const int y = row == 3 ? 210 : 126 + row * 25;
                if (PointInside(input.touchX, input.touchY, 16, y - 3, 288, 24)) {
                    sUpdate.row = row;
                    UpdateAction(row);
                    break;
                }
            }
        }
    }
    if (input.downMask) sUi.bottomDirty = true;
}

void UpdateText(const char* text, float center, float y, float scale, uint32_t color, float width) {
    scale = std::min(scale, width / std::max(1.0f, MeasureText(text, 1.0f)));
    DrawText(text, center, y, scale, color, C2D_AlignCenter, 0.85f, true);
}

void DrawUpdateBottom() {
    RefreshUpdateStatus();
    if (sUi.game.racing) DrawRaceBackground(); else DrawDimMenuBackground();
    const auto white = C2D_Color32(242, 241, 220, 255);
    const auto yellow = C2D_Color32(255, 225, 75, 255);
    const auto green = C2D_Color32(167, 255, 151, 255);
    UpdateText("UPDATE", 160, 17, 1.12f, yellow, 284);
    if (sUpdate.confirm) {
        UpdateText("INSTALL AND CLOSE?", 160, 80, 0.86f, yellow, 288);
        UpdateText("UNSAVED PROGRESS WILL BE LOST", 160, 112, 0.65f, white, 288);
        UpdateText("YES", 96, 170, 0.95f, sUpdate.confirmYes ? yellow : white, 70);
        UpdateText("NO", 224, 170, 0.95f, sUpdate.confirmYes ? white : yellow, 70);
        DrawTexture(sUi.selectionTriangle, sUpdate.confirmYes ? 59 : 190, 174, 12, 7, 0.86f);
        UpdateText("B BACK", 160, 216, 0.68f, white, 288);
        return;
    }
    char label[112];
    std::snprintf(label, sizeof(label), "INSTALLED  %s", MK64_3DS_VERSION);
    UpdateText(label, 160, 48, 0.78f, white, 288);
    std::snprintf(label, sizeof(label), "LATEST  %s", sUpdate.status.version[0] ? sUpdate.status.version : "--");
    UpdateText(label, 160, 69, 0.78f, white, 288);
    UpdateText(sUpdate.status.message[0] ? sUpdate.status.message : "CHECK FOR A NEW VERSION",
               160, 96, 0.68f, sUpdate.status.state == UPDATE_ERROR ? yellow : green, 290);
    if (Updater_Busy() || sUpdate.status.state == UPDATE_DONE) {
        C2D_DrawRectSolid(28, 132, 0.7f, 264, 10, C2D_Color32(35, 35, 35, 255));
        C2D_DrawRectSolid(30, 134, 0.8f, 260 * sUpdate.status.progress / 100.0f, 6, yellow);
        if (sUpdate.status.state != UPDATE_CHECKING) {
            std::snprintf(label, sizeof(label), "%u%%", sUpdate.status.progress);
            UpdateText(label, 160, 154, 0.9f, white, 288);
        }
        UpdateText(sUpdate.status.state == UPDATE_INSTALLING ? "DO NOT POWER OFF" :
                   sUpdate.status.state == UPDATE_DONE ? "CLOSING GAME" : "B CANCEL",
                   160, 213, 0.72f, white, 288);
        return;
    }
    const char* rows[] = { sUpdate.status.prerelease ? "CHANNEL: EXPERIMENTAL" : "CHANNEL: STABLE",
                          "CHECK FOR UPDATES", "INSTALL UPDATE", "BACK" };
    for (unsigned row = 0; row < 4; ++row) {
        const float y = row == 3 ? 210 : 126 + row * 25;
        const bool enabled = row != 2 || sUpdate.status.state == UPDATE_AVAILABLE;
        const auto color = !enabled ? C2D_Color32(145, 145, 135, 255) : sUpdate.row == row ? yellow : white;
        UpdateText(rows[row], 160, y, 0.78f, color, 266);
        if (sUpdate.row == row) DrawTexture(sUi.selectionTriangle, 11, y + 5, 12, 7, 0.86f);
    }
}

void RefreshUpdateNotes() {
    if (sUpdate.notesRevision == sUpdate.status.revision) return;
    sUpdate.notesRevision = sUpdate.status.revision;
    char notes[12289] = {};
    Updater_GetNotes(notes, sizeof(notes));
    if (!notes[0]) {
        FILE* f = std::fopen("romfs:/update-changelog.txt", "rb");
        if (f) { std::fread(notes, 1, sizeof(notes) - 1, f); std::fclose(f); }
    }
    if (sUpdate.lineCount && !std::strcmp(notes, sUpdate.notes)) return;
    std::snprintf(sUpdate.notes, sizeof(sUpdate.notes), "%s", notes);
    char lines[384][43];
    const unsigned count = Update_FormatNotes(notes, lines, 384);
    sUpdate.lineCount = 0;
    // The game's letters have variable advances; wrap against pixels too.
    for (unsigned i = 0; i < count && sUpdate.lineCount < 384; ++i) {
        const char* p = lines[i];
        do {
            unsigned take = 0, space = 0;
            float width = 0;
            while (p[take] && width + CharacterAdvance(p[take]) * 0.70f <= 354) {
                width += CharacterAdvance(p[take]) * 0.70f;
                if (p[take] == ' ') space = take;
                ++take;
            }
            if (p[take] && space) take = space;
            auto& line = sUpdate.lines[sUpdate.lineCount++];
            std::memcpy(line, p, take); line[take] = 0;
            p += take;
            while (*p == ' ') ++p;
        } while (*p && sUpdate.lineCount < 384);
    }
    sUpdate.page = 0;
}

void DrawUpdateTop() {
    RefreshUpdateNotes();
    // Stock menu/HUD foregrounds are omitted while Update is open.
    // Dim the remaining clean menu background or current 3D race scene.
    C2D_DrawRectSolid(0, 0, 0.3f, 400, 240, C2D_Color32(0, 0, 0, 204));
    const auto white = C2D_Color32(242, 241, 220, 255);
    const auto yellow = C2D_Color32(255, 225, 75, 255);
    UpdateText("CHANGELOG", 200, 14, 1.0f, yellow, 360);
    for (unsigned row = 0; row < kUpdateLinesPerPage; ++row) {
        const unsigned i = sUpdate.page * kUpdateLinesPerPage + row;
        if (i >= sUpdate.lineCount) break;
        DrawText(sUpdate.lines[i], 17, 43 + row * 15, 0.70f, white, C2D_AlignLeft, 0.85f, true);
    }
    char page[48];
    std::snprintf(page, sizeof(page), "PAGE %u / %u", sUpdate.page + 1,
                  std::max(1U, (sUpdate.lineCount + kUpdateLinesPerPage - 1) / kUpdateLinesPerPage));
    UpdateText("L", 24, 219, 0.72f, yellow, 24);
    UpdateText(page, 200, 219, 0.72f, yellow, 300);
    UpdateText("R", 376, 219, 0.72f, yellow, 24);
}
