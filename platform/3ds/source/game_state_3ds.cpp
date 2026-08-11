#include "game_state_3ds.h"

#include "engine/AllTracks.h"
#include "engine/Cup.h"
#include "engine/World.h"
#include "engine/registry/RegisterContent.h"
#include "engine/sky/Sky.h"
#include "port/Game.h"

#include "assets/textures/other_textures.h"
#include "assets/textures/player_selection.h"
#include "assets/textures/texture_tkmk00.h"

#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "code_80005FD0.h"
#include "main.h"
#include "menus.h"
#include "save.h"
}

#include <array>
#include <cstring>
#include <memory>
#include <string>

extern std::unique_ptr<Cup> gMushroomCup;
extern std::unique_ptr<Cup> gFlowerCup;
extern std::unique_ptr<Cup> gStarCup;
extern std::unique_ptr<Cup> gSpecialCup;
extern std::unique_ptr<Cup> gBattleCup;
extern std::unique_ptr<Sky> gSky;

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

size_t sTrackIndex = 0;
int8_t sLastTopHudEnabled = -1;

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
    sTrackIndex = index;
    kTracks[index].select();
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
    return true;
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
    Cup* cup = GetWorld() == nullptr ? nullptr : GetWorld()->GetCurrentCup();
    if (cup != nullptr) {
        TrackBrowser_SetTrack(cup->GetTrack().c_str());
    }
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

extern "C" void Mk64GameState3DSGetBottomUISnapshot(Mk64BottomUIGameState3DS* snapshot) {
    if (snapshot == nullptr) return;
    std::memset(snapshot, 0, sizeof(*snapshot));
    std::fill(std::begin(snapshot->standingPlayerIds), std::end(snapshot->standingPlayerIds), int8_t{-1});
    std::fill(std::begin(snapshot->standingCharacterIds), std::end(snapshot->standingCharacterIds), int8_t{-1});
    for (Mk64BottomUIRacer3DS& racer : snapshot->racers) {
        racer.characterId = -1;
        racer.rank = -1;
    }

    snapshot->gameState = gGamestate;
    snapshot->gameMode = gModeSelection;
    snapshot->menuSelection = gMenuSelection;
    snapshot->mainMenuSelection = gMainMenuSelection;
    snapshot->gameSelectVisible =
        gMenuSelection == MAIN_MENU && gMainMenuSelection == MAIN_MENU_PLAYER_SELECT;
    snapshot->racing = gGamestate == RACING;
    snapshot->paused = snapshot->racing && gIsGamePaused != 0;
    snapshot->mirrorMode = gIsMirrorMode != 0;
    snapshot->courseTimerSeconds = gCourseTimer;
    snapshot->currentItem = gPlayers[0].currentItemCopy;
    snapshot->totalLaps = 3;
    const int lap = std::clamp(gLapCountByPlayerId[0] + 1, 1, 3);
    snapshot->currentLap = static_cast<int8_t>(lap);

    snapshot->trackIndex = std::min(sTrackIndex, kTracks.size() - 1);
    const VanillaTrack& track = kTracks[snapshot->trackIndex];
    snapshot->trackName = track.name;
    snapshot->coursePreviewTexture = track.preview;
    snapshot->minimapTexture = track.minimap;
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
    }

    if (snapshot->gameMode == BATTLE) {
        // Battle does not maintain the Grand Prix rank table. Present active
        // participants honestly instead of stale "TOP 5" standings.
        for (size_t playerId = 0;
             playerId < MK64_BOTTOM_UI_RACER_COUNT &&
             snapshot->standingCount < MK64_BOTTOM_UI_STANDING_COUNT;
             ++playerId) {
            if (!snapshot->racers[playerId].active) continue;
            snapshot->standingPlayerIds[snapshot->standingCount] =
                static_cast<int8_t>(playerId);
            snapshot->standingCharacterIds[snapshot->standingCount] =
                snapshot->racers[playerId].characterId;
            ++snapshot->standingCount;
        }
    } else {
        for (size_t rank = 0; rank < MK64_BOTTOM_UI_STANDING_COUNT; ++rank) {
            const int playerId = gGPCurrentRacePlayerIdByRank[rank];
            if (playerId < 0 || playerId >= MK64_BOTTOM_UI_RACER_COUNT ||
                !snapshot->racers[playerId].active) {
                continue;
            }
            snapshot->standingPlayerIds[snapshot->standingCount] = static_cast<int8_t>(playerId);
            snapshot->standingCharacterIds[snapshot->standingCount] =
                snapshot->racers[playerId].characterId;
            ++snapshot->standingCount;
        }
    }

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
    snapshot->minimapRed = properties->Minimap.Colour.r;
    snapshot->minimapGreen = properties->Minimap.Colour.g;
    snapshot->minimapBlue = properties->Minimap.Colour.b;
}

extern "C" void Mk64GameState3DSSetTopHudEnabled(bool enabled) {
    const int8_t value = enabled ? 1 : 0;
    if (sLastTopHudEnabled == value) return;
    sLastTopHudEnabled = value;
    CVarSetInteger("gDrawHUD", enabled ? 1 : 0);
}

extern "C" void Mk64GameState3DSApplyTurbo(bool active, uint8_t multiplier) {
    const int clampedMultiplier = std::clamp<int>(multiplier, 1, 5);
    // The unmodified game executes two 30 Hz logic ticks for each rendered
    // key frame. User-facing x1 must preserve that baseline.
    gTickLogic = active ? 2 * clampedMultiplier : 2;
}
