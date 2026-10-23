    file(READ "${SPAGHETTIKART_TOADS_TURNPIKE_SOURCE}" SPAGHETTIKART_3DS_TOADS_TURNPIKE_TEXT)
    set(SPAGHETTIKART_3DS_TOADS_TURNPIKE_ORIGINAL
        "${SPAGHETTIKART_3DS_TOADS_TURNPIKE_TEXT}")
    string(REPLACE
        "        a /= 2; // Normally vehicle logic is only ran every 2 frames. This slows the vehicles down to match.\n        b /= 2;"
        "        // PLATFORM_3DS restores the original every-other-tick cadence in World::TickActors."
        SPAGHETTIKART_3DS_TOADS_TURNPIKE_TEXT
        "${SPAGHETTIKART_3DS_TOADS_TURNPIKE_TEXT}"
    )
    if(SPAGHETTIKART_3DS_TOADS_TURNPIKE_TEXT STREQUAL
       SPAGHETTIKART_3DS_TOADS_TURNPIKE_ORIGINAL)
        message(FATAL_ERROR "The upstream Toad's Turnpike traffic speed compensation changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_TOADS_TURNPIKE_SOURCE}"
         "${SPAGHETTIKART_3DS_TOADS_TURNPIKE_TEXT}")

    set(SPAGHETTIKART_TRACK_SOURCE "${SPAGHETTIKART_ROOT}/src/engine/tracks/Track.cpp")
    set(SPAGHETTIKART_3DS_TRACK_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/Track_3ds.cpp")
    list(REMOVE_ITEM SPAGHETTIKART_ENGINE_TRACKS "${SPAGHETTIKART_TRACK_SOURCE}")
    file(READ "${SPAGHETTIKART_TRACK_SOURCE}" SPAGHETTIKART_TRACK_TEXT)
    string(REPLACE
        "extern \"C\" {\n#include \"main.h\""
        "extern \"C\" {\nvoid mk64_3ds_collision_reset_visits(void);\n#include \"main.h\""
        SPAGHETTIKART_3DS_TRACK_TEXT
        "${SPAGHETTIKART_TRACK_TEXT}"
    )
    set(SPAGHETTIKART_3DS_TRACK_DECL_TEXT "${SPAGHETTIKART_3DS_TRACK_TEXT}")
    if(SPAGHETTIKART_3DS_TRACK_DECL_TEXT STREQUAL SPAGHETTIKART_TRACK_TEXT)
        message(FATAL_ERROR "The upstream Track collision-reset declaration insertion point changed")
    endif()
    string(REPLACE
        "    gCollisionMeshCount = 0;\n    gCollisionMesh = (CollisionTriangle*) gNextFreeMemoryAddress;"
        "    gCollisionMeshCount = 0;\n    mk64_3ds_collision_reset_visits();\n    gCollisionMesh = (CollisionTriangle*) gNextFreeMemoryAddress;"
        SPAGHETTIKART_3DS_TRACK_TEXT
        "${SPAGHETTIKART_3DS_TRACK_TEXT}"
    )
    if(SPAGHETTIKART_3DS_TRACK_TEXT STREQUAL SPAGHETTIKART_3DS_TRACK_DECL_TEXT)
        message(FATAL_ERROR "The upstream Track collision-reset call insertion point changed")
    endif()
    if(MK64_3DS_NATIVE_FULL_GAME)
        string(PREPEND SPAGHETTIKART_3DS_TRACK_TEXT "#include \"native_program_3ds.h\"\n")
        # Record the exact winding mutation producer, including packed-child
        # selection. The native procedures never inspect modified gfx words.
        string(FIND "${SPAGHETTIKART_3DS_TRACK_TEXT}" "void InvertTriangleWindingModdedInternal(" mirrorEnd)
        string(SUBSTRING "${SPAGHETTIKART_3DS_TRACK_TEXT}" 0 ${mirrorEnd} mirrorOriginal)
        string(REPLACE "        gModifiedGfxSet.insert(nameStr);"
            "        gModifiedGfxSet.insert(nameStr);\n        Mk64NativeProgramSetMirror(nameStr.c_str(),shouldSwap);" mirrorChanged "${mirrorOriginal}")
        string(REPLACE "                    shouldSwap = (strstr(name, \"packed\") != nullptr);"
            "                    shouldSwap = (strstr(name, \"packed\") != nullptr);\n                    Mk64NativeProgramSetMirror(nameStr.c_str(),shouldSwap);" mirrorChanged "${mirrorChanged}")
        string(REPLACE "${mirrorOriginal}" "${mirrorChanged}" SPAGHETTIKART_3DS_TRACK_TEXT "${SPAGHETTIKART_3DS_TRACK_TEXT}")
        string(REPLACE "    gModifiedGfxSet.clear();" "    gModifiedGfxSet.clear();\n    Mk64NativeProgramResetMirrors();"
            SPAGHETTIKART_3DS_TRACK_TEXT "${SPAGHETTIKART_3DS_TRACK_TEXT}")
    endif()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(WRITE "${SPAGHETTIKART_3DS_TRACK_SOURCE}" "${SPAGHETTIKART_3DS_TRACK_TEXT}")

    file(READ "${SPAGHETTIKART_LUIGI_RACEWAY_SOURCE}"
         SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT)
    set(SPAGHETTIKART_3DS_LUIGI_RACEWAY_ORIGINAL
        "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT}")
    string(REPLACE
        "#include <memory>"
        "#include <memory>\n#include \"jumbotron_3ds.h\""
        SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT
        "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT}")
    if(SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT STREQUAL
       SPAGHETTIKART_3DS_LUIGI_RACEWAY_ORIGINAL)
        message(FATAL_ERROR "The upstream Luigi Raceway include block changed")
    endif()
    set(SPAGHETTIKART_3DS_LUIGI_RACEWAY_INCLUDE_TEXT
        "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT}")
    string(REPLACE
        [=[        CopyJumbotron(D_800DC5DC, D_800DC5E0, currentScreenSection,
                                            (u16*) gPortFramebuffers[prevFrame]);]=]
        [=[        Mk64Jumbotron3DSUpdate(
            (u16*) LOAD_ASSET_RAW(gTexture68272C),
            (u16*) LOAD_ASSET_RAW(gTexture682928),
            (u16*) LOAD_ASSET_RAW(gTexture682B24),
            (u16*) LOAD_ASSET_RAW(gTexture682D20),
            (u16*) LOAD_ASSET_RAW(gTexture682F1C),
            (u16*) LOAD_ASSET_RAW(gTexture683118));]=]
        SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT
        "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT}"
    )
    if(SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT STREQUAL
       SPAGHETTIKART_3DS_LUIGI_RACEWAY_INCLUDE_TEXT)
        message(FATAL_ERROR "The upstream Luigi Raceway jumbotron copy call changed")
    endif()
    # Every Luigi race geometry pass is native in this experiment. The six mutable
    # billboard images publish revisions directly, so they have no legacy
    # texture-cache consumers to invalidate. Keep the old path for comparison.
    if(MK64_3DS_NATIVE_LUIGI_LAB AND NOT MK64_3DS_NATIVE_COMPARE_LAB)
        foreach(nativeJumbo 68272C 682928 682B24 682D20 682F1C 683118)
            set(nativeInvalidate "    gSPInvalidateTexCache(gDisplayListHead++, (uintptr_t) gTexture${nativeJumbo});")
            string(FIND "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT}" "${nativeInvalidate}" nativeLocation)
            if(nativeLocation LESS 0)
                message(FATAL_ERROR "Native Luigi mutable image invalidation anchor changed")
            endif()
            string(REPLACE "${nativeInvalidate}" "" SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT}")
        endforeach()
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_SOURCE}"
         "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT}")

    file(READ "${SPAGHETTIKART_WARIO_STADIUM_SOURCE}"
         SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT)
    set(SPAGHETTIKART_3DS_WARIO_STADIUM_ORIGINAL
        "${SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT}")
    string(REPLACE
        "#include <memory>"
        "#include <memory>\n#include \"jumbotron_3ds.h\""
        SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT
        "${SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT}")
    if(SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT STREQUAL
       SPAGHETTIKART_3DS_WARIO_STADIUM_ORIGINAL)
        message(FATAL_ERROR "The upstream Wario Stadium include block changed")
    endif()
    set(SPAGHETTIKART_3DS_WARIO_STADIUM_INCLUDE_TEXT
        "${SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT}")
    string(REPLACE
        "        CopyJumbotron(D_800DC5DC, D_800DC5E0, currentScreenSection, (u16*) gPortFramebuffers[prevFrame]);"
        [=[        Mk64Jumbotron3DSUpdate(
            (u16*) LOAD_ASSET_RAW(gTexture68272C),
            (u16*) LOAD_ASSET_RAW(gTexture682928),
            (u16*) LOAD_ASSET_RAW(gTexture682B24),
            (u16*) LOAD_ASSET_RAW(gTexture682D20),
            (u16*) LOAD_ASSET_RAW(gTexture682F1C),
            (u16*) LOAD_ASSET_RAW(gTexture683118));]=]
        SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT
        "${SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT}"
    )
    if(SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT STREQUAL
       SPAGHETTIKART_3DS_WARIO_STADIUM_INCLUDE_TEXT)
        message(FATAL_ERROR "The upstream Wario Stadium jumbotron copy call changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_WARIO_STADIUM_SOURCE}"
         "${SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT}")

    file(GLOB SPAGHETTIKART_ENGINE_VEHICLES CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/engine/vehicles/*.cpp"
    )
    set(SPAGHETTIKART_ENGINE_REGISTRY
        "${SPAGHETTIKART_ROOT}/src/engine/registry/RegisterItems.cpp"
        "${SPAGHETTIKART_ROOT}/src/engine/registry/RegisterItemTables.cpp"
    )
    set(SPAGHETTIKART_GAME_BRIDGE_SOURCE "${SPAGHETTIKART_ROOT}/src/port/Game.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${SPAGHETTIKART_GAME_BRIDGE_SOURCE}")
    set(SPAGHETTIKART_3DS_GAME_BRIDGE_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/Game_3ds.cpp")
    file(READ "${SPAGHETTIKART_GAME_BRIDGE_SOURCE}" SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT)
    set(SPAGHETTIKART_3DS_GAME_BRIDGE_ORIGINAL "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    string(REPLACE
        [=[                } else {
                    GetWorld()->GetTrack()->DrawCredits();
                }
            case RENDER_TRACK_SECTIONS:]=]
        [=[                } else {
                    GetWorld()->GetTrack()->DrawCredits();
                }
                break;
            case RENDER_TRACK_SECTIONS:]=]
        SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}"
    )
    if(SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT STREQUAL
       SPAGHETTIKART_3DS_GAME_BRIDGE_ORIGINAL)
        message(FATAL_ERROR "The upstream full-scene render switch changed")
    endif()
    set(SPAGHETTIKART_3DS_GAME_VEHICLE_COLLISION_ORIGINAL
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    string(REPLACE
        [=[void CM_VehicleCollision(s32 playerId, Player* player) {
    for (auto& actor : GetWorld()->Actors) {
        if (actor) {
            actor->VehicleCollision(playerId, player);
        }
    }
}]=]
        [=[void CM_VehicleCollision(s32 playerId, Player* player) {
    for (auto& actor : GetWorld()->Actors) {
        if (actor && actor->IsMod()) {
            actor->VehicleCollision(playerId, player);
        }
    }
}]=]
        SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}"
    )
    if(SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT STREQUAL
       SPAGHETTIKART_3DS_GAME_VEHICLE_COLLISION_ORIGINAL)
        message(FATAL_ERROR "The upstream vehicle-collision dispatch changed")
    endif()
    set(SPAGHETTIKART_3DS_GAME_TRACK_RELEASE_ORIGINAL
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    string(REPLACE
        "#include <atomic>"
        "#include <atomic>\n#include <array>\n#include <cstdio>\n#include <cstring>\n#include \"libultraship/bridge/resourcebridge.h\""
        SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}"
    )
    string(REPLACE
        [=[void LoadTrack() {
    GetWorld()->GetRaceManager().Load();
}]=]
        [=[extern "C" void Mk64Diagnostics3DSRaceResourceBoundary(const char*, const char*);
void LoadTrack() {
    GetWorld()->GetRaceManager().Load();
    Track* track = GetWorld()->GetTrack();
    if (track != nullptr) {
        Mk64Diagnostics3DSRaceResourceBoundary("after-track-load", track->ResourceName.c_str());
    }
}]=]
        SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}"
    )
    if(MK64_3DS_NATIVE_FULL_GAME)
        string(PREPEND SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT "extern \"C\" void Mk64NativeCourseRelease();\nextern \"C\" void Mk64NativeCourseLoad(const char*);\nextern \"C\" void Mk64GameAudio3DSBeginLoading();\nextern \"C\" void Mk64GameAudio3DSEndLoading();\n")
        string(REPLACE "void LoadTrack() {\n    GetWorld()->GetRaceManager().Load();"
            "void LoadTrack() {\n    struct LoadingAudioScope {\n        LoadingAudioScope() { Mk64GameAudio3DSBeginLoading(); }\n        ~LoadingAudioScope() { Mk64GameAudio3DSEndLoading(); }\n    } loadingAudio;\n    Mk64NativeCourseRelease();\n    GetWorld()->GetRaceManager().Load();"
            SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
        string(REPLACE "        Mk64Diagnostics3DSRaceResourceBoundary(\"after-track-load\", track->ResourceName.c_str());"
            "        Mk64NativeCourseLoad(track->ResourceName.c_str());\n        Mk64Diagnostics3DSRaceResourceBoundary(\"after-track-load\", track->ResourceName.c_str());"
            SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    endif()
    if(SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT STREQUAL
       SPAGHETTIKART_3DS_GAME_TRACK_RELEASE_ORIGINAL)
        message(FATAL_ERROR "The upstream track-load resource boundary changed")
    endif()
    if(NOT MK64_3DS_NATIVE_FULL_GAME)
        string(PREPEND SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
            "#include \"audio_runtime_3ds.h\"\n")
        string(REPLACE "void LoadTrack() {\n    GetWorld()->GetRaceManager().Load();"
            "void LoadTrack() {\n    struct LoadingAudioScope {\n        LoadingAudioScope() { Mk64GameAudio3DSBeginLoading(); }\n        ~LoadingAudioScope() { Mk64GameAudio3DSEndLoading(); }\n    } loadingAudio;\n    GetWorld()->GetRaceManager().Load();"
            SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    endif()
    set(SPAGHETTIKART_3DS_CAMERA_CLEAN_ORIGINAL
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    string(REPLACE
        [=[void CM_CleanCameras(void) {
    GetWorld()->Cameras.clear();
}]=]
        [=[void CM_CleanCameras(void) {
    // Screen contexts do not own these cameras. Clear every alias before the
    // owning vector so a pending switch cannot reactivate a freed camera on
    // the first tick of the next race or results transition.
    for (ScreenContext& screen : gScreenContexts) {
        screen.camera = nullptr;
        screen.raceCamera = nullptr;
        screen.lookBehindCamera = nullptr;
        screen.freeCamera = nullptr;
        screen.pendingCamera = nullptr;
    }
    GetWorld()->Cameras.clear();
}]=]
        SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}"
    )
    if(SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT STREQUAL
       SPAGHETTIKART_3DS_CAMERA_CLEAN_ORIGINAL)
        message(FATAL_ERROR "The upstream camera cleanup boundary changed")
    endif()
    # The original game tested gCurrentCourseId; SpaghettiKart replaced each
    # test with a dynamic_cast of the active track. devkitARM's type_info
    # equality falls back to strcmp, and AI/effects/collision code issues
    # these checks for every racer on every tick. A vtable pointer identifies
    # the complete dynamic type, so caching one cast result per vtable is exact.
    mk64_auto_replace(SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
        "// clang-format off\nbool IsMarioRaceway()"
        [=[extern "C++" {
namespace {
template <typename T> bool Mk64TrackIs() {
    static std::atomic<uintptr_t> cached{ 0 };
    const Track* track = GetWorld()->GetTrack();
    if (track == nullptr) {
        return false;
    }
    const uintptr_t vtable = *reinterpret_cast<const uintptr_t*>(track);
    const uintptr_t entry = cached.load(std::memory_order_relaxed);
    if ((entry & ~uintptr_t{ 1 }) == vtable) {
        return (entry & 1u) != 0;
    }
    const bool match = dynamic_cast<const T*>(track) != nullptr;
    cached.store(vtable | (match ? 1u : 0u), std::memory_order_relaxed);
    return match;
}
} // namespace
} // extern "C++"

// clang-format off
bool IsMarioRaceway()]=])
    string(REGEX REPLACE
        [=[bool (Is[A-Za-z]+)\(\) +{ return dynamic_cast<([A-Za-z]+)\*>\(GetWorld\(\)->GetTrack\(\)\) != nullptr; }]=]
        [=[bool \1() { return Mk64TrackIs<\2>(); }]=]
        SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    string(FIND "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}" "dynamic_cast<PodiumCeremony*>(GetWorld()->GetTrack())" MK64_TRACK_CAST_LEFT)
    string(FIND "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}" "bool IsMarioRaceway() { return Mk64TrackIs<MarioRaceway>(); }" MK64_TRACK_CAST_FIRST)
    string(FIND "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}" "bool IsPodiumCeremony() { return Mk64TrackIs<PodiumCeremony>(); }" MK64_TRACK_CAST_LAST)
    if(NOT MK64_TRACK_CAST_LEFT EQUAL -1 OR MK64_TRACK_CAST_FIRST LESS 0 OR MK64_TRACK_CAST_LAST LESS 0)
        message(FATAL_ERROR "The upstream track identity checks changed")
    endif()
    string(APPEND SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT "\n#include \"turnpike_traffic_3ds.inc.hpp\"\n")
    mk64_auto_replace(SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
        "    for (size_t i = 0; i < gPlayerCountSelection1; i++) {\n        // Retry does not respawn actors, therefore, re-use lakitu."
        "    for (size_t i = 0; i < (Mk64SoloBattle3DS() ? 4 : gPlayerCountSelection1); i++) {\n        // Retry does not respawn actors, therefore, re-use lakitu.")
    mk64_auto_replace(SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
        "void CM_DrawBattleBombKarts(s32 cameraId) {\n    for (size_t i = 0; i < gPlayerCount; i++) {"
        "void CM_DrawBattleBombKarts(s32 cameraId) {\n    for (size_t i = 0; i < (Mk64SoloBattle3DS() ? 4 : gPlayerCount); i++) {")
    set(SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT "#include \"solo_modes_3ds.h\"\n${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    file(WRITE "${SPAGHETTIKART_3DS_GAME_BRIDGE_SOURCE}"
         "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    set_source_files_properties("${SPAGHETTIKART_3DS_GAME_BRIDGE_SOURCE}" PROPERTIES
        GENERATED TRUE
        COMPILE_DEFINITIONS "main=spaghettikart_desktop_main;Graphics_PushFrame=Graphics_PushFrameDesktop;CustomEngineInit=DesktopCustomEngineInit;CustomEngineDestroy=DesktopCustomEngineDestroy"
    )
