#include "game_state_3ds.h"

#include "engine/AllTracks.h"
#include "engine/Cup.h"
#include "engine/World.h"
#include "engine/registry/RegisterContent.h"
#include "engine/sky/Sky.h"
#include "port/Game.h"

#include "assets/textures/other_textures.h"

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
    void (*select)();
};

const std::array<VanillaTrack, 20> kTracks = {{
    { "mk:mario_raceway", "mario raceway", "m circuit", "567m", minimap_mario_raceway, SelectMarioRaceway },
    { "mk:choco_mountain", "choco mountain", "mountain", "687m", minimap_choco_mountain, SelectChocoMountain },
    { "mk:bowsers_castle", "bowser's castle", "castle", "777m", minimap_bowsers_castle, SelectBowsersCastle },
    { "mk:banshee_boardwalk", "banshee boardwalk", "ghost", "747m", minimap_banshee_boardwalk, SelectBansheeBoardwalk },
    { "mk:yoshi_valley", "yoshi valley", "maze", "772m", minimap_yoshi_valley, SelectYoshiValley },
    { "mk:frappe_snowland", "frappe snowland", "snow", "734m", minimap_frappe_snowland, SelectFrappeSnowland },
    { "mk:koopa_troopa_beach", "koopa troopa beach", "beach", "691m", minimap_koopa_troopa_beach, SelectKoopaTroopaBeach },
    { "mk:royal_raceway", "royal raceway", "p circuit", "1025m", minimap_royal_raceway, SelectRoyalRaceway },
    { "mk:luigi_raceway", "luigi raceway", "l circuit", "717m", minimap_luigi_raceway, SelectLuigiRaceway },
    { "mk:moo_moo_farm", "moo moo farm", "farm", "527m", minimap_moo_moo_farm, SelectMooMooFarm },
    { "mk:toads_turnpike", "toad's turnpike", "highway", "1036m", minimap_toads_turnpike, SelectToadsTurnpike },
    { "mk:kalimari_desert", "kalimari desert", "desert", "753m", minimap_kalimari_desert, SelectKalimariDesert },
    { "mk:sherbet_land", "sherbet land", "sherbet", "756m", minimap_sherbet_land, SelectSherbetLand },
    { "mk:rainbow_road", "rainbow road", "rainbow", "2000m", minimap_rainbow_road, SelectRainbowRoad },
    { "mk:wario_stadium", "wario stadium", "stadium", "1591m", minimap_wario_stadium, SelectWarioStadium },
    { "mk:block_fort", "block fort", "block", "", minimap_block_fort, SelectBlockFort },
    { "mk:skyscraper", "skyscraper", "skyscraper", "", minimap_skyscraper, SelectSkyscraper },
    { "mk:double_deck", "double deck", "deck", "", minimap_double_deck, SelectDoubleDeck },
    { "mk:dk_jungle", "d.k.'s jungle parkway", "jungle", "893m", minimap_dks_jungle_parkway, SelectDkJungle },
    { "mk:big_donut", "big donut", "doughnut", "", minimap_big_donut, SelectBigDonut },
}};

size_t sTrackIndex = 0;

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
