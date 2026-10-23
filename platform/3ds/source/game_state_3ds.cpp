#include "settings_3ds.h"
#include "race_clock_3ds.hpp"
#include "render_policy_3ds.hpp"
#include "game_state_3ds.h"
#include "bottom_ui_3ds.h"
#include "hud_layout_3ds.hpp"
#include "input_policy_3ds.hpp"
#include "menu_transition_3ds.hpp"
#include "random_cup_3ds.hpp"
#include "resource_runtime_3ds.h"
#include "resource_loading_3ds.h"
#include "game_runtime_3ds.h"
#include "diagnostics_3ds.h"
#include "auto_mode_3ds.h"
#include "battle_standings_3ds.hpp"

#include "engine/AllTracks.h"
#include "engine/Cup.h"
#include "engine/World.h"
#include "engine/registry/RegisterContent.h"
#include "engine/sky/Sky.h"
#include "port/Game.h"

#include "assets/textures/other_textures.h"
#include "assets/textures/player_selection.h"
#include "assets/textures/texture_tkmk00.h"
#include "assets/models/data_segment2.h"

#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "audio/external.h"
#include "code_80005FD0.h"
#include "code_800029B0.h"
#include "code_80057C60.h"
#include "main.h"
#include "menu_items.h"
#include "menus.h"
#include "objects.h"
#include "racing/race_logic.h"
#include "save.h"
#include "save_data.h"
#include "sounds.h"
#include "cpu_hit_state_3ds.inc.h"

extern const int8_t D_800F0B54[];
}

#include <array>
#include <cstring>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

extern std::unique_ptr<Cup> gMushroomCup;
extern std::unique_ptr<Cup> gFlowerCup;
extern std::unique_ptr<Cup> gStarCup;
extern std::unique_ptr<Cup> gSpecialCup;
extern std::unique_ptr<Cup> gBattleCup;
extern std::unique_ptr<Sky> gSky;

extern "C" bool Mk64GameState3DSGetPauseTextColors(bool selected, uint32_t* top, uint32_t* bottom) {
    if (!top || !bottom) return false;
    const auto* vertices = static_cast<const Vtx*>(LOAD_ASSET(D_02007BB8));
    if (!vertices) return false;
    // Native text_rainbow_effect: yellow idle, cycle 2 while selected.
    const int index = selected ? 6 + (gGlobalTimer % 2) : TEXT_YELLOW;
    const auto rgba = [](const Vtx& vertex) {
        const auto* c = vertex.v.cn;
        return uint32_t(c[0]) | (uint32_t(c[1]) << 8) |
            (uint32_t(c[2]) << 16) | (uint32_t(c[3]) << 24);
    };
    *top = rgba(vertices[0]);
    *bottom = rgba(vertices[(index + 1) * 2]);
    return true;
}

namespace {
struct VanillaTrack {
    const char* resourceName;
    const char* name;
    const char* debugName;
    const char* length;
    const char* minimap;
    const char* preview;
    void (*select)();
};

const std::array<VanillaTrack, 20> kTracks = {{
    { "mk:mario_raceway", "mario raceway", "m circuit", "567m", minimap_mario_raceway,
      gTextureCoursePreviewMarioRaceway, SelectMarioRaceway },
    { "mk:choco_mountain", "choco mountain", "mountain", "687m", minimap_choco_mountain,
      gTextureCoursePreviewChocoMountain, SelectChocoMountain },
    { "mk:bowsers_castle", "bowser's castle", "castle", "777m", minimap_bowsers_castle,
      gTextureCoursePreviewBowsersCastle, SelectBowsersCastle },
    { "mk:banshee_boardwalk", "banshee boardwalk", "ghost", "747m", minimap_banshee_boardwalk,
      gTextureCoursePreviewBansheeBoardwalk, SelectBansheeBoardwalk },
    { "mk:yoshi_valley", "yoshi valley", "maze", "772m", minimap_yoshi_valley,
      gTextureCoursePreviewYoshiValley, SelectYoshiValley },
    { "mk:frappe_snowland", "frappe snowland", "snow", "734m", minimap_frappe_snowland,
      gTextureCoursePreviewFrappeSnowland, SelectFrappeSnowland },
    { "mk:koopa_troopa_beach", "koopa troopa beach", "beach", "691m", minimap_koopa_troopa_beach,
      gTextureCoursePreviewKoopaTroopaBeach, SelectKoopaTroopaBeach },
    { "mk:royal_raceway", "royal raceway", "p circuit", "1025m", minimap_royal_raceway,
      gTextureCoursePreviewRoyalRaceway, SelectRoyalRaceway },
    { "mk:luigi_raceway", "luigi raceway", "l circuit", "717m", minimap_luigi_raceway,
      gTextureCoursePreviewLuigiRaceway, SelectLuigiRaceway },
    { "mk:moo_moo_farm", "moo moo farm", "farm", "527m", minimap_moo_moo_farm,
      gTextureCoursePreviewMooMooFarm, SelectMooMooFarm },
    { "mk:toads_turnpike", "toad's turnpike", "highway", "1036m", minimap_toads_turnpike,
      gTextureCoursePreviewToadsTurnpike, SelectToadsTurnpike },
    { "mk:kalimari_desert", "kalimari desert", "desert", "753m", minimap_kalimari_desert,
      gTextureCoursePreviewKalimariDesert, SelectKalimariDesert },
    { "mk:sherbet_land", "sherbet land", "sherbet", "756m", minimap_sherbet_land,
      gTextureCoursePreviewSherbetLand, SelectSherbetLand },
    { "mk:rainbow_road", "rainbow road", "rainbow", "2000m", minimap_rainbow_road,
      gTextureCoursePreviewRainbowRoad, SelectRainbowRoad },
    { "mk:wario_stadium", "wario stadium", "stadium", "1591m", minimap_wario_stadium,
      gTextureCoursePreviewWarioStadium, SelectWarioStadium },
    { "mk:block_fort", "block fort", "block", "", minimap_block_fort,
      gTextureCoursePreviewBlockFort, SelectBlockFort },
    { "mk:skyscraper", "skyscraper", "skyscraper", "", minimap_skyscraper,
      gTextureCoursePreviewSkyscraper, SelectSkyscraper },
    { "mk:double_deck", "double deck", "deck", "", minimap_double_deck,
      gTextureCoursePreviewDoubleDeck, SelectDoubleDeck },
    { "mk:dk_jungle", "d.k.'s jungle parkway", "jungle", "893m", minimap_dks_jungle_parkway,
      gTextureCoursePreviewDksJungleParkway, SelectDkJungle },
    { "mk:big_donut", "big donut", "doughnut", "", minimap_big_donut,
      gTextureCoursePreviewBigDonut, SelectBigDonut },
}};
constexpr std::array<const char*, 20> kTrackAssetDirectories = {{
    "mario_raceway", "choco_mountain", "bowsers_castle", "banshee_boardwalk",
    "yoshi_valley", "frappe_snowland", "koopa_troopa_beach", "royal_raceway",
    "luigi_raceway", "moo_moo_farm", "toads_turnpike", "kalimari_desert",
    "sherbet_land", "rainbow_road", "wario_stadium", "block_fort", "skyscraper",
    "double_deck", "dks_jungle_parkway", "big_donut",
}};
constexpr std::array<uint8_t, 8> kNativePlaceGreen = {
    255, 237, 215, 191, 162, 130, 97, 58,
};

size_t sTrackIndex = 0;
std::array<mk64_3ds::AutoHudMotion, MK64_BOTTOM_UI_RACER_COUNT> sAutoHudMotion{};
std::array<mk64_3ds::PortraitSpin3DS, MK64_BOTTOM_UI_RACER_COUNT> sPortraitSpin{};
mk64_3ds::BattleStandings3DS sBattleStandings;
struct AutoLapTimes { uint32_t times[3]{}, previous=0; int completed=0; };
std::array<AutoLapTimes, MK64_BOTTOM_UI_RACER_COUNT> sAutoLapTimes{};
bool sSelectedMirrorMode = false;
int8_t sLastTopHudEnabled = -1;
std::unique_ptr<Cup> sRandomCup;
std::unique_ptr<mk64_3ds::RandomCupPlan> sRandomPlan;
Cup* sPreviousCup = nullptr;
size_t sPreviousCupIndex = 0, sPreviousCupCursor = 0;
int sPreviousCC = CC_150, sRandomPreparedRace = -1;
bool sRandomDriverAndSpeed = false, sReturningToTitle = false;

const char* TrackAssetDirectory(std::string_view resourceName) {
    if (resourceName == "mk:podium_ceremony") {
        return "podium_ceremony";
    }
    for (size_t index = 0; index < kTracks.size(); ++index) {
        if (resourceName == kTracks[index].resourceName) {
            return kTrackAssetDirectories[index];
        }
    }
    return nullptr;
}

void RegisterVanillaTracks() {
    gTrackRegistry.Clear();
    for (const VanillaTrack& track : kTracks) {
        TrackInfo info = {
            .ResourceName = track.resourceName,
            .Name = track.name,
            .DebugName = track.debugName,
            .Length = track.length,
            .MinimapTexture = track.minimap,
        };
        gTrackRegistry.Add(info, [select = track.select]() { select(); });
    }

    // The ceremony is not selectable in the browser's 20-course index, but
    // it still goes through Track::Load after a Grand Prix. Keep it in the
    // registry so that path initializes its collision arena as well.
    TrackInfo podium = {
        .ResourceName = "mk:podium_ceremony",
        .Name = "podium ceremony",
        .DebugName = "podium",
        .Length = "1025m",
        .MinimapTexture = nullptr,
    };
    gTrackRegistry.Add(podium, []() { SelectPodiumCeremony(); });
}

void SelectIndex(size_t index) {
    if (index >= kTracks.size()) return;
    World* world = GetWorld();
    Track* currentTrack = world == nullptr ? nullptr : world->GetTrack();
    if (index == sTrackIndex && currentTrack != nullptr &&
        currentTrack->ResourceName == kTracks[index].resourceName &&
        sSelectedMirrorMode == (gIsMirrorMode != 0)) {
        return;
    }
    sTrackIndex = index;
    kTracks[index].select();
    sSelectedMirrorMode = gIsMirrorMode != 0;
}
}

extern "C" bool Mk64GameState3DSInit() {
    sLastTopHudEnabled = -1;
    gSky = std::make_unique<Sky>();
    RegisterVanillaTracks();

    gMushroomCup = std::make_unique<Cup>("mk:mushroom_cup", "Mushroom Cup",
        std::vector<std::string>{ "mk:luigi_raceway", "mk:moo_moo_farm", "mk:koopa_troopa_beach", "mk:kalimari_desert" });
    gFlowerCup = std::make_unique<Cup>("mk:flower_cup", "Flower Cup",
        std::vector<std::string>{ "mk:toads_turnpike", "mk:frappe_snowland", "mk:choco_mountain", "mk:mario_raceway" });
    gStarCup = std::make_unique<Cup>("mk:star_cup", "Star Cup",
        std::vector<std::string>{ "mk:wario_stadium", "mk:sherbet_land", "mk:royal_raceway", "mk:bowsers_castle" });
    gSpecialCup = std::make_unique<Cup>("mk:special_cup", "Special Cup",
        std::vector<std::string>{ "mk:dk_jungle", "mk:yoshi_valley", "mk:banshee_boardwalk", "mk:rainbow_road" });
    gBattleCup = std::make_unique<Cup>("mk:battle_cup", "Battle Cup",
        std::vector<std::string>{ "mk:big_donut", "mk:block_fort", "mk:double_deck", "mk:skyscraper" });

    gMushroomCup->ValidateTrackIds(gTrackRegistry);
    gFlowerCup->ValidateTrackIds(gTrackRegistry);
    gStarCup->ValidateTrackIds(gTrackRegistry);
    gSpecialCup->ValidateTrackIds(gTrackRegistry);
    gBattleCup->ValidateTrackIds(gTrackRegistry);

    World* world = GetWorld();
    if (world == nullptr) return false;
    world->Cups.clear();
    world->AddCup(gMushroomCup.get());
    world->AddCup(gFlowerCup.get());
    world->AddCup(gStarCup.get());
    world->AddCup(gSpecialCup.get());
    world->AddCup(gBattleCup.get());

    RegisterItems(gItemRegistry);
    RegisterItemTables(gItemTableRegistry);
    SetMarioRaceway();
    sTrackIndex = 0;
    sSelectedMirrorMode = gIsMirrorMode != 0;
    return true;
}

extern "C" void Mk64GameState3DSPrepareRaceResources(void) {
    // Close before any load/presentation; native viewport opening follows later.
    Mk64BottomUI3DSSetRaceOpening(0.0f);
    Mk64RaceClock3DSReset();
    sAutoHudMotion = {};
    sPortraitSpin = {};
    sBattleStandings = {};
    sAutoLapTimes = {};
    World* world = GetWorld();
    Track* track = world == nullptr ? nullptr : world->GetTrack();
    if (track == nullptr || TrackAssetDirectory(track->ResourceName) == nullptr) return;
    std::array<char, 64> selected = {};
    std::snprintf(selected.data(), selected.size(), "%s", track->ResourceName.c_str());
    Mk64Diagnostics3DSRaceResourceBoundary("before-reset", selected.data());
    Mk64Graphics3DSResetForTrackLoad();
    // load_track already destroyed scene actors. Cameras and the selected
    // Track can still retain raw waypoint/texture pointers, so destroy them
    // before releasing decoded data, including on a same-course restart.
    CM_CleanCameras();
    world->SetCurrentTrack(nullptr);
    Mk64Resource3DSReleaseRaceResources();
    Mk64Diagnostics3DSRaceResourceBoundary("after-release", selected.data());
    Mk64Resource3DSPrepareCourse(TrackAssetDirectory(selected.data()));
    if (std::strcmp(selected.data(), "mk:podium_ceremony") == 0) {
        SelectPodiumCeremony();
    } else {
        for (const auto& entry : kTracks) {
            if (std::strcmp(selected.data(), entry.resourceName) == 0) {
                entry.select();
                break;
            }
        }
    }
    Mk64Diagnostics3DSRaceResourceBoundary("after-track-constructor", selected.data());
}

extern "C" void TrackBrowser_SetTrack(const char* name) {
    if (name == nullptr) return;
    for (size_t index = 0; index < kTracks.size(); ++index) {
        if (std::strcmp(name, kTracks[index].resourceName) == 0) {
            SelectIndex(index);
            return;
        }
    }
}

extern "C" void TrackBrowser_SetTrackFromCup() {
    World* world = GetWorld();
    Cup* cup = world == nullptr ? nullptr : world->GetCurrentCup();
    if (cup == nullptr || cup->CursorPosition >= cup->mTracks.size()) return;

    // menus.c asks for the selected cup track every frame. Borrow the existing
    // cup string and avoid reconstructing a Track (and all of its resource
    // vectors) while the selection has not changed.
    const std::string& resourceName = cup->mTracks[cup->CursorPosition];
    if (cup == sRandomCup.get() && sRandomPlan && cup->CursorPosition < 4)
        gCurrentCourseId = sRandomPlan->tracks[cup->CursorPosition];
    Track* currentTrack = world->GetTrack();
    if (currentTrack != nullptr && currentTrack->ResourceName == resourceName &&
        sSelectedMirrorMode == (gIsMirrorMode != 0)) {
        return;
    }
    TrackBrowser_SetTrack(resourceName.c_str());
}

extern "C" bool Mk64RandomCup3DSActive() { return sRandomCup != nullptr; }

extern "C" int Mk64RandomCup3DSMenuKind() {
    if (gGamestate < START_MENU_FROM_QUIT || gGamestate > COURSE_SELECT_MENU_FROM_QUIT || sRandomCup) return 0;
    if (gMenuSelection == MAIN_MENU) return 1;
    return gMenuSelection == COURSE_SELECT_MENU && gModeSelection == GRAND_PRIX ? 2 : 0;
}

extern "C" bool Mk64RandomCup3DSStart(bool randomDriverAndSpeed, uint64_t seed) {
    World* world = GetWorld();
    if (!world || !Mk64RandomCup3DSMenuKind() || gFadeModeSelection != FADE_MODE_NONE ||
        is_screen_being_faded()) return false;
    sPreviousCup = world->GetCurrentCup();
    sPreviousCupIndex = world->GetCupIndex();
    sPreviousCupCursor = sPreviousCup ? sPreviousCup->CursorPosition : 0;
    sPreviousCC = gCCSelection;
    sRandomDriverAndSpeed = randomDriverAndSpeed;
    sRandomPreparedRace = -1;
    sRandomPlan = std::make_unique<mk64_3ds::RandomCupPlan>(seed);
    std::vector<std::string> tracks;
    for (uint8_t track : sRandomPlan->tracks) tracks.emplace_back(kTracks[track].resourceName);
    sRandomCup = std::make_unique<Cup>("mk:random_cup", "RANDOM CUP", std::move(tracks));
    world->SetCupIndex(MUSHROOM_CUP); // Native array accesses remain in bounds.
    world->SetCurrentCup(sRandomCup.get());
    gCupSelection = MUSHROOM_CUP;
    gCourseIndexInCup = 0;
    D_800DC540 = MUSHROOM_CUP;
    D_800DC544 = 0;
    gPlayerCount = gPlayerCountSelection1 = 1;
    gScreenModeSelection = SCREEN_MODE_1P;
    gModeSelection = GRAND_PRIX;
    gDemoMode = 0;
    gIsGamePaused = 0;
    gEnableDebugMode = 0;
    gDebugMenuSelection = DEBUG_MENU_DISABLED;
    if (randomDriverAndSpeed) gCharacterSelections[0] = sRandomPlan->character;
    gMenuSelection = COURSE_SELECT_MENU;
    gSubMenuSelection = SUB_MENU_MAP_SELECT_OK;
    Mk64RandomCup3DSPrepareRace();
    TrackBrowser_SetTrackFromCup();
    func_8009E1C0();
    func_800CA330(25);
    play_sound2(SOUND_MENU_OK_CLICKED);
    return true;
}

extern "C" void Mk64RandomCup3DSPrepareRace() {
    if (!sRandomCup || !sRandomPlan || (gGamestate != RACING && gMenuSelection != COURSE_SELECT_MENU)) return;
    const int race = sRandomCup->CursorPosition;
    if (race < 0 || race >= 4 || race == sRandomPreparedRace) return;
    sRandomPreparedRace = race;
    if (sRandomDriverAndSpeed) gCCSelection = sRandomPlan->NextMirror() ? CC_EXTRA : CC_150;
    set_mirror_mode(gCCSelection == CC_EXTRA);
}

extern "C" void Mk64RandomCup3DSReset() {
    if (!sRandomCup) return;
    World* world = GetWorld();
    if (world && sPreviousCup) {
        world->SetCurrentCup(sPreviousCup);
        world->SetCupIndex(sPreviousCupIndex);
        sPreviousCup->CursorPosition = sPreviousCupCursor;
    }
    gCupSelection = sPreviousCupIndex;
    gCourseIndexInCup = sPreviousCupCursor;
    gCCSelection = sPreviousCC;
    set_mirror_mode(gCCSelection == CC_EXTRA);
    sRandomCup.reset();
    sRandomPlan.reset();
    sPreviousCup = nullptr;
    sRandomPreparedRace = -1;
}

extern "C" bool Mk64GameState3DSSkipCeremony() {
    if (gGamestate != ENDING || is_screen_being_faded()) return false;
    MenuItem* item = find_menu_items(MENU_ITEM_TYPE_12B);
    if (!item) {
        add_menu_item(MENU_ITEM_TYPE_12B, 0, 0, MENU_ITEM_PRIORITY_4);
        item = find_menu_items(MENU_ITEM_TYPE_12B);
    }
    if (!item || item->state >= 6) return false;
    item->state = 6; // Native exit: same audio fade and Special Cup -> staff route.
    item->param1 = 0;
    play_sound2(SOUND_MENU_OK_CLICKED);
    return true;
}

extern "C" void Mk64MenuBackTitle3DSBegin() { sReturningToTitle = true; }
extern "C" int Mk64MenuBackTitle3DSFadeIn(int defaultDuration) {
    const bool returning = sReturningToTitle && gMenuSelection == START_MENU;
    sReturningToTitle = false;
    return returning ? 30 : defaultDuration;
}

extern "C" void TrackBrowser_NextTrack() { SelectIndex((sTrackIndex + 1) % kTracks.size()); }
extern "C" void TrackBrowser_PreviousTrack() { SelectIndex((sTrackIndex + kTracks.size() - 1) % kTracks.size()); }
extern "C" size_t TrackBrowser_GetTrackIndex() { return sTrackIndex; }
extern "C" const char* TrackBrowser_GetTrackName() { return kTracks[sTrackIndex].name; }
extern "C" const char* TrackBrowser_GetTrackDebugName() { return kTracks[sTrackIndex].debugName; }
extern "C" const char* TrackBrowser_GetTrackLength() { return kTracks[sTrackIndex].length; }
extern "C" void TrackBrowser_SetTrackByIdx(size_t index) { SelectIndex(index); }
extern "C" const char* TrackBrowser_GetTrackNameByIdx(size_t index) { return index < kTracks.size() ? kTracks[index].name : ""; }
extern "C" const char* TrackBrowser_GetTrackDebugNameByIdx(size_t index) { return index < kTracks.size() ? kTracks[index].debugName : ""; }
extern "C" const char* TrackBrowser_GetTrackLengthByIdx(size_t index) { return index < kTracks.size() ? kTracks[index].length : ""; }
extern "C" const char* TrackBrowser_GetMinimapTextureByIdx(size_t index) { return index < kTracks.size() ? kTracks[index].minimap : nullptr; }

extern "C" void Mk64GameState3DSUpdateHudAnimations(void) {
    // Called once per simulation preparation, never by a snapshot getter or
    // midpoint presentation. Pause freezes motion, and camera changes do not
    // restart a driver's animation. No playerHUD[4..7] access for AUTO.
    if (gGamestate != RACING || gIsGamePaused) return;
    for (int id = 0; id < MK64_BOTTOM_UI_RACER_COUNT; ++id) {
        if (!(gPlayers[id].type & PLAYER_EXISTS)) {
            sPortraitSpin[id] = {};
            continue;
        }
        sPortraitSpin[id].Tick(Mk64RacerHitMotion3DS(&gPlayers[id]));
        if (!Mk64Auto3DSActive()) continue;
        sAutoHudMotion[id].Tick(Mk64CpuItem3DSHud(id) != ITEM_NONE, gLapCountByPlayerId[id] >= 3);
        auto& laps=sAutoLapTimes[id];
        const int completed=std::clamp<int>(gLapCountByPlayerId[id],0,3);
        if (completed>laps.completed) {
            // CPU racers have no native viewport HUD. Retain the original
            // interpolated finish-line timestamp, never a separate clock.
            const auto centis=static_cast<uint32_t>(std::max(0.f,gTimePlayerLastTouchedFinishLine[id])*100);
            laps.times[completed-1]=centis>=laps.previous?centis-laps.previous:0;
            laps.previous=centis; laps.completed=completed;
        }
    }
}

extern "C" void Mk64GameState3DSGetBottomUISnapshot(Mk64BottomUIGameState3DS* snapshot) {
    if (snapshot == nullptr) return;
    std::memset(snapshot, 0, sizeof(*snapshot));
    std::fill(std::begin(snapshot->standingPlayerIds), std::end(snapshot->standingPlayerIds), int8_t{-1});
    std::fill(std::begin(snapshot->standingCharacterIds), std::end(snapshot->standingCharacterIds), int8_t{-1});
    for (Mk64BottomUIRacer3DS& racer : snapshot->racers) {
        racer.characterId = -1;
        racer.rank = -1;
    }

    snapshot->topHudOpacity = gGamestate == RACING && gRaceState >= RACE_IN_PROGRESS
        ? std::clamp(gCourseTimer, 0.0f, 1.0f) : 0.0f;
    snapshot->dataGrid = gGamestate != RACING && gMenuSelection == DATA_MENU;
    snapshot->dataCourse = gGamestate != RACING && gMenuSelection == COURSE_DATA_MENU;
    snapshot->dataIndex = std::clamp<int>(gTimeTrialDataCourseIndex, 0, 15);
    snapshot->dataSubmenu = gSubMenuSelection;
    snapshot->dataConfirm = gCourseRecordsSubMenuSelection;
    snapshot->dataEraseOption = gCourseRecordsMenuSelection;
    if (snapshot->dataGrid || snapshot->dataCourse) {
        snapshot->dataHasGhost = func_800B639C(snapshot->dataIndex) >= 0;
        snapshot->dataHasRecords = gSaveData.allCourseTimeTrialRecords.cupRecords[snapshot->dataIndex / 4]
            .courseRecords[snapshot->dataIndex % 4].unknownBytes[0] != 0;
        for (int i = 0; i < 6; ++i) snapshot->dataTimes[i] = i < 5
            ? func_800B4EB4(i, snapshot->dataIndex) : func_800B4FB0(snapshot->dataIndex);
    }
    snapshot->gameState = gGamestate;
    snapshot->hudAnimationTick = static_cast<uint32_t>(gGlobalTimer);
    snapshot->menuFlashTick = static_cast<uint32_t>(gCycleFlashMenu);
    snapshot->gameMode = gModeSelection;
    snapshot->menuSelection = gMenuSelection;
    snapshot->mainMenuSelection = gMainMenuSelection;
    snapshot->selectedDriver = static_cast<int8_t>(std::clamp<int>(gCharacterSelections[0],0,7));
    snapshot->selectedLevel = static_cast<int8_t>(std::clamp<int>(gCCSelection,CC_50,CC_EXTRA));
    snapshot->gameSelectVisible =
        gMenuSelection == MAIN_MENU && gMainMenuSelection == MAIN_MENU_PLAYER_SELECT;
    snapshot->gameSelectOptionReveal = snapshot->gameSelectDataReveal = 0.0f;
    if (gMenuSelection == MAIN_MENU) {
        if (const auto* item = find_menu_items(MAIN_MENU_OPTION_GFX))
            snapshot->gameSelectOptionReveal = mk64_3ds::GameSelectReveal(item->visible != 0, item->param1, item->column, 21);
        if (const auto* item = find_menu_items(MAIN_MENU_DATA_GFX))
            snapshot->gameSelectDataReveal = mk64_3ds::GameSelectReveal(item->visible != 0, item->param1, item->column, 85);
    }
    snapshot->racing = gGamestate == RACING;
    snapshot->demo = gDemoMode != 0;
    snapshot->autoMode = Mk64Auto3DSActive();
    snapshot->autoCinematic = Mk64Auto3DSCinematic();
    snapshot->autoMenuAvailable = Mk64Auto3DSMenuAvailable();
    const int observed = Mk64Auto3DSObservedPlayer();
    snapshot->observedPlayer = static_cast<int8_t>(observed);
    // Native single-player results create two camera windows by changing
    // gPlayerCountSelection1 to 2. That is not a second controller/player.
    snapshot->multiplayer = gPlayerCount > 1;
    snapshot->paused = snapshot->racing && gIsGamePaused != 0;
    snapshot->mirrorMode = gIsMirrorMode != 0;
    snapshot->topHudRenderMode = static_cast<int8_t>(Mk64GameState3DSGetTopHudRenderMode());
    // Path completion is authoritative at the crossing; the HUD animation can
    // lag it. Do not retain an item frame during the post-finish camera.
    snapshot->raceFinished = snapshot->racing &&
        (gModeSelection == BATTLE ? gRaceState >= RACE_FINISHED :
         (gLapCountByPlayerId[observed] >= 3 || (!snapshot->autoMode && playerHUD[0].lapCount == 3)));
    // Native result-window conversion starts at phase 1, before the vertical
    // exit. This boundary hides upper place only; lower HUD stays intact.
    snapshot->topFinishPlaceVisible = snapshot->raceFinished &&
        D_8015F894 == 0 && gRaceState != RACE_EXIT;
    snapshot->finishPortraitTransfer = snapshot->topFinishPlaceVisible &&
        (gModeSelection == GRAND_PRIX || gModeSelection == VERSUS);
    snapshot->finishPortraitLineup = D_801657E2 != 0;
    snapshot->finishPortraitPhase = static_cast<uint16_t>(D_8018D1CC);
    if (!snapshot->autoMode)
        std::copy(std::begin(playerHUD[0].lapDurations), std::end(playerHUD[0].lapDurations),
                  snapshot->lapDurationCentiseconds);
    else
        std::copy(std::begin(sAutoLapTimes[observed].times),std::end(sAutoLapTimes[observed].times),
                  snapshot->lapDurationCentiseconds);
    // Native playerHUD has FOUR viewport slots, not eight racer slots. CPUs
    // use their race/path/strategy data directly, including drivers 5--8.
    snapshot->courseTimerCentiseconds = playerHUD[0].someTimer;
    if (snapshot->autoMode) snapshot->courseTimerCentiseconds = static_cast<uint32_t>(
        (snapshot->raceFinished ? gTimePlayerLastTouchedFinishLine[observed] : gCourseTimer) * 100.0f);
    snapshot->courseTimerSeconds = static_cast<float>(snapshot->courseTimerCentiseconds) / 100.0f;
    snapshot->currentItem = gPlayers[observed].currentItemCopy;
    const int itemWindowObject = gItemWindowObjectByPlayerId[observed];
    if (!snapshot->autoMode && itemWindowObject >= 0 && itemWindowObject < OBJECT_LIST_SIZE) {
        const Object& itemObject = gObjectList[itemWindowObject];
        snapshot->itemWindowVisible = itemObject.state >= 2;
        snapshot->itemWindowSlideY = playerHUD[0].slideItemBoxY;
        snapshot->itemTextureIndex = static_cast<uint8_t>(
            std::clamp<int>(itemObject.textureListIndex, 0, 15));
    }
    if (snapshot->autoMode) {
        snapshot->currentItem = Mk64CpuItem3DSHud(observed);
        snapshot->itemTextureIndex = static_cast<uint8_t>(snapshot->currentItem);
        snapshot->itemWindowVisible = sAutoHudMotion[observed].itemVisible;
        snapshot->itemWindowSlideY = sAutoHudMotion[observed].itemSlideY;
    }
    snapshot->totalLaps = 3;
    const int lap = std::clamp(gLapCountByPlayerId[observed] + 1, 1, 3);
    snapshot->currentLap = static_cast<int8_t>(lap);

    snapshot->trackIndex = std::min(sTrackIndex, kTracks.size() - 1);
    if (snapshot->dataGrid || snapshot->dataCourse)
        snapshot->trackIndex = gCupCourseOrder[snapshot->dataIndex / 4][snapshot->dataIndex % 4];
    const VanillaTrack& track = kTracks[snapshot->trackIndex];
    snapshot->trackLength = track.length;
    snapshot->trackName = track.name;
    snapshot->coursePreviewTexture = track.preview;
    snapshot->minimapTexture = track.minimap;
    // DATA does not load a racing World/Properties. Preserve the two native
    // blue vanilla outlines there too; race snapshots use live Properties.
    const bool blueOutline = snapshot->trackIndex == TRACK_FRAPPE_SNOWLAND ||
                             snapshot->trackIndex == TRACK_SHERBET_LAND;
    snapshot->minimapRed = blueOutline ? 72 : 255;
    snapshot->minimapGreen = blueOutline ? 100 : 255;
    snapshot->minimapBlue = 255;
    snapshot->mainBackgroundTexture = has_unlocked_extra_mode() != 0 ? background_sunset : background_blue_sky;

    if (!snapshot->racing) return;

    for (size_t playerId = 0; playerId < MK64_BOTTOM_UI_RACER_COUNT; ++playerId) {
        const Player& player = gPlayers[playerId];
        Mk64BottomUIRacer3DS& racer = snapshot->racers[playerId];
        racer.active = (player.type & PLAYER_EXISTS) != 0;
        if (!racer.active) continue;
        ++snapshot->activeRacerCount;
        racer.characterId = player.characterId < 8
                                ? static_cast<int8_t>(player.characterId)
                                : int8_t{-1};
        racer.rank = player.currentRank >= 0 && player.currentRank < MK64_BOTTOM_UI_RACER_COUNT
                         ? static_cast<int8_t>(player.currentRank)
                         : int8_t{-1};
        racer.worldX = player.pos[0];
        racer.worldZ = player.pos[2];
        racer.rotationY = player.rotation[1];
        racer.portraitHitAngle = sPortraitSpin[playerId].Angle();
    }

    Mk64GameState3DSRefreshStandings(snapshot);
    snapshot->currentPlaceVisible = snapshot->raceFinished || snapshot->autoMode || playerHUD[0].unk_81 != 0;
    snapshot->currentPlaceIndex = static_cast<uint8_t>(
        std::clamp<int>(snapshot->autoMode || snapshot->raceFinished ? gGPCurrentRaceRankByPlayerId[observed]
                                                   : D_8018CF98[observed],
                        0, 7));
    const int placeColorIndex = snapshot->raceFinished
                                    ? std::clamp<int>(D_80165594, 0, 7)
                                    : snapshot->currentPlaceIndex;
    snapshot->currentPlaceGreen = kNativePlaceGreen[placeColorIndex];
    snapshot->currentPlaceScale = snapshot->autoMode ? sAutoHudMotion[observed].placeScale : playerHUD[0].rankScaling;
    snapshot->currentPlaceNativeX = playerHUD[0].rankX + playerHUD[0].slideRankX;
    snapshot->currentPlaceNativeY = playerHUD[0].rankY + playerHUD[0].slideRankY;

    Properties* properties = CM_GetProps();
    if (properties == nullptr) return;
    snapshot->minimapTexture = properties->Minimap.Texture != nullptr
                                   ? properties->Minimap.Texture
                                   : track.minimap;
    snapshot->minimapWidth = properties->Minimap.Width;
    snapshot->minimapHeight = properties->Minimap.Height;
    snapshot->minimapPlayerX = properties->Minimap.PlayerX;
    snapshot->minimapPlayerY = properties->Minimap.PlayerY;
    snapshot->minimapPlayerScale = properties->Minimap.PlayerScaleFactor;
    snapshot->minimapFinishlineX = properties->Minimap.FinishlineX;
    snapshot->minimapFinishlineY = properties->Minimap.FinishlineY;
    // Preserve the owner's native minimap colours on both screens.
    snapshot->minimapRed = properties->Minimap.Colour.r;
    snapshot->minimapGreen = properties->Minimap.Colour.g;
    snapshot->minimapBlue = properties->Minimap.Colour.b;
}

extern "C" void Mk64GameState3DSRefreshStandings(Mk64BottomUIGameState3DS* snapshot) {
    if (snapshot == nullptr || !snapshot->racing || gGamestate != RACING) return;
    // PrepareFrame precedes the game simulation. Refresh this small live set
    // again at presentation so the portrait and its border use ONE rank table.
    snapshot->standingCount = 0;
    snapshot->observedPlayer = static_cast<int8_t>(std::clamp<int>(Mk64Auto3DSObservedPlayer(), 0, 7));
    for (int id = 0; id < MK64_BOTTOM_UI_RACER_COUNT; ++id)
        snapshot->racers[id].rank = static_cast<int8_t>(std::clamp<int>(gPlayers[id].currentRank, 0, 7));
    if (snapshot->gameMode == BATTLE) {
        std::array<int, 4> balloons;
        for (int id = 0; id < 4; ++id)
            balloons[id] = (gPlayers[id].type & PLAYER_EXISTS) ? gPlayerBalloonCount[id] : -1;
        sBattleStandings.Update(balloons);
        // Arena ranking is independent of the racing path/rival rank tables.
        // Retain eliminated portraits below survivors even after bomb impact.
        for (const int playerId : sBattleStandings.order) {
            if (gPlayerCount > 1 && playerId >= gPlayerCount) continue;
            snapshot->standingPlayerIds[snapshot->standingCount] =
                static_cast<int8_t>(playerId);
            snapshot->standingCharacterIds[snapshot->standingCount] =
                static_cast<int8_t>(std::clamp<int>(gPlayers[playerId].characterId, 0, 7));
            snapshot->racers[playerId].rank = static_cast<int8_t>(snapshot->standingCount);
            snapshot->standingLapCounts[snapshot->standingCount] = 0;
            snapshot->standingNativeX[snapshot->standingCount] = 40.0f;
            snapshot->standingNativeY[snapshot->standingCount] =
                35.0f + 32.0f * snapshot->standingCount;
            snapshot->standingNativeDirection[snapshot->standingCount] = 0.0f;
            ++snapshot->standingCount;
        }
    } else {
        for (size_t rank = 0; rank < MK64_BOTTOM_UI_STANDING_COUNT; ++rank) {
            const int playerId = gGPCurrentRacePlayerIdByRank[rank];
            if (playerId < 0 || playerId >= MK64_BOTTOM_UI_RACER_COUNT ||
                !(gPlayers[playerId].type & PLAYER_EXISTS)) {
                continue;
            }
            snapshot->standingPlayerIds[snapshot->standingCount] = static_cast<int8_t>(playerId);
            snapshot->standingCharacterIds[snapshot->standingCount] = static_cast<int8_t>(
                std::clamp<int>(gPlayers[playerId].characterId, 0, 7));
            snapshot->standingLapCounts[snapshot->standingCount] =
                static_cast<int8_t>(gLapCountByPlayerId[playerId]);
            snapshot->standingUnknown[snapshot->standingCount] =
                IsYoshiValley() && gLapCountByPlayerId[playerId] < 3;
            snapshot->standingNativeX[snapshot->standingCount] = D_8018D028[rank];
            snapshot->standingNativeY[snapshot->standingCount] = D_8018D050[rank];
            snapshot->standingNativeDirection[snapshot->standingCount] = D_8018D078[rank];
            ++snapshot->standingCount;
        }
    }

    snapshot->standingAlpha = static_cast<uint8_t>(std::clamp<int>(D_8018D3E0, 0, 255));
    snapshot->playerBorderRed = static_cast<uint8_t>(std::clamp<int>(D_8018D3E4, 0, 255));
    snapshot->playerBorderGreen = static_cast<uint8_t>(std::clamp<int>(D_8018D3E8, 0, 255));
    snapshot->playerBorderBlue = static_cast<uint8_t>(std::clamp<int>(D_8018D3EC, 0, 255));
}

extern "C" void Mk64GameState3DSSetTopHudEnabled(bool enabled) {
    const int8_t value = (enabled ? 1 : 0) | (gPlayerCountSelection1 > 1 ? 2 : 0);
    if (sLastTopHudEnabled == value) return;
    sLastTopHudEnabled = value;
    CVarSetInteger("gDrawHUD", gPlayerCountSelection1 > 1 ? 1 : 0);
}

extern "C" bool Mk64GameState3DSRaceControlsActive() {
    return gGamestate == RACING && gIsGamePaused == 0;
}

extern "C" int Mk64GameState3DSGetTopHudRenderMode() {
    if (gGamestate != RACING || gIsGamePaused != 0) return MK64_TOP_HUD_RENDER_NONE;
    // AUTO's HUD is composed for the watched CPU by the port, never player 0.
    if (Mk64Auto3DSActive()) return MK64_TOP_HUD_RENDER_NONE;
    if (gDemoMode != 0 || gLapCountByPlayerId[0] >= 3 || playerHUD[0].lapCount == 3)
        return MK64_TOP_HUD_RENDER_NONE;
    // Multi-player retains each viewport's native HUD. Single-player Classic
    // is composed at native display resolution by the port UI.
    if (gPlayerCountSelection1 > 1) return MK64_TOP_HUD_RENDER_FULL;
    const auto meters = Mk64Settings3DSGetHudMeters();
    return mk64_3ds::HudMetersAvailable(Mk64Settings3DSGetHudLayout(), meters)
        ? static_cast<int>(meters) : MK64_TOP_HUD_RENDER_NONE;
}

extern "C" bool Mk64GameState3DSPerformPauseAction(Mk64PauseAction3DS action) {
    if (gGamestate != RACING || gIsGamePaused == 0) return false;
    MenuItem* pauseItem = find_menu_items(MENU_ITEM_PAUSE);
    if (pauseItem == nullptr) return false;
    if (action == MK64_PAUSE_ACTION_CONTINUE) {
        pauseItem->state = 0;
        gIsGamePaused = 0;
        func_8028DF38();
        func_800C9F90(0);
        return true;
    }
    if (action == MK64_PAUSE_ACTION_QUIT) {
        const int mode = std::clamp<int>(gModeSelection, 0, 3);
        pauseItem->state = D_800F0B54[mode];
        func_8009DFE0(30);
        play_sound2(SOUND_ACTION_CONTINUE_UNKNOWN);
        func_800CA330(60);
        return true;
    }
    return false;
}

extern "C" void Mk64GameState3DSApplyTurbo(bool active, uint8_t multiplier) {
    const int clampedMultiplier = std::clamp<int>(multiplier, 1, 5);
    // The unmodified game executes two 30 Hz logic ticks for each rendered
    // key frame. User-facing x1 must preserve that baseline.
    gTickLogic = active ? 2 * clampedMultiplier : 2;
    Mk64RaceClock3DSSetSpeed(active ? clampedMultiplier : 1);
}

// Called once per presentation, after the world is ready. High and all menu
// states preserve the stock renderer. Perspective scale is 1 in GameCamera.
extern "C" float Mk64GameState3DSDistanceFog(uint32_t* color) {
    if (gGamestate != RACING || Mk64Settings3DSGetRenderDistance() == MK64_RENDER_DISTANCE_3DS_HIGH)
        return 0.0f;
    Properties* properties = CM_GetProps();
    if (properties == nullptr) return 0.0f;
    const auto horizon = properties->Skybox.BottomRight;
    const uint8_t r = bFog ? gFogColour.r : horizon.r;
    const uint8_t g = bFog ? gFogColour.g : horizon.g;
    const uint8_t b = bFog ? gFogColour.b : horizon.b;
    *color = r | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16) | 0xFF000000U;
    return mk64_3ds::RenderDistanceEnd(properties->FarPersp, Mk64Settings3DSGetRenderDistance());
}


extern "C" const char* Mk64GameState3DSDataPreview(int index) {
    index = std::clamp(index, 0, 15);
    return kTracks[gCupCourseOrder[index / 4][index % 4]].preview;
}
extern "C" const char* Mk64GameState3DSDataName(int index) {
    index = std::clamp(index, 0, 15);
    return kTracks[gCupCourseOrder[index / 4][index % 4]].name;
}
extern "C" void Mk64GameState3DSDataMove(int delta) {
    if (is_screen_being_faded()) return;
    gTimeTrialDataCourseIndex = std::clamp<int>(gTimeTrialDataCourseIndex + delta, 0, 15);
    play_sound2(SOUND_MENU_CURSOR_MOVE);
}
extern "C" void Mk64GameState3DSDataConfirm(int direction, bool activate, bool back) {
    Controller input{};
    input.buttonPressed = (direction < 0 ? U_JPAD : direction > 0 ? D_JPAD : 0) |
        (activate ? A_BUTTON : 0) | (back ? B_BUTTON : 0);
    course_data_menu_act(&input, 0);
}
extern "C" void Mk64GameState3DSDataAction(int action) {
    if (is_screen_being_faded()) return;
    if (gMenuSelection == DATA_MENU) {
        if (action == -1) func_8009E258();
        else { gCourseRecordsMenuSelection = COURSE_RECORDS_MENU_RETURN_MENU; func_8009E1C0(); }
        play_sound2(SOUND_MENU_OK_CLICKED);
        return;
    }
    if (gMenuSelection != COURSE_DATA_MENU) return;
    const int index = std::clamp<int>(gTimeTrialDataCourseIndex, 0, 15);
    if (action == -1 || action == 0) { func_8009E208(); play_sound2(SOUND_MENU_GO_BACK); return; }
    const bool ghost = func_800B639C(index) >= 0;
    if ((action == 2 || action == 3) && !ghost) return;
    if (action == 3) {
        gModeSelection = TIME_TRIALS;
        gPlayerCount = gPlayerCountSelection1 = 1;
        gScreenModeSelection = SCREEN_MODE_1P;
        gCCSelection = CC_100;
        gIsMirrorMode = 0;
        CM_SetCupIndex(index / 4);
        SetCupCursorPosition(index % 4);
        gCupSelection = index / 4;
        gCourseIndexInCup = index % 4;
        gCurrentCourseId = gCupCourseOrder[index / 4][index % 4];
        TrackBrowser_SetTrackFromCup();
        // Follow the normal course-selection fade, which initializes and loads
        // the saved ghost through the existing Time Trial startup path.
        gMenuSelection = COURSE_SELECT_MENU;
        gSubMenuSelection = SUB_MENU_MAP_SELECT_OK;
        func_8009E1C0();
        func_800CA330(0x19);
        play_sound2(SOUND_MENU_OK_CLICKED);
        return;
    }
    if (action == 1 && !gSaveData.allCourseTimeTrialRecords.cupRecords[index / 4]
        .courseRecords[index % 4].unknownBytes[0]) return;
    gCourseRecordsMenuSelection = action;
    gSubMenuSelection = SUB_MENU_DATA_ERASE_CONFIRM;
    gCourseRecordsSubMenuSelection = COURSE_RECORDS_SUB_MENU_QUIT;
    play_sound2(SOUND_MENU_SELECT);
}
