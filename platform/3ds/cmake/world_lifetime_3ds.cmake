    # SpaghettiKart advances two 60 Hz logic ticks for every 30 Hz key frame,
    # but its replacement Toad's Turnpike traffic currently performs the full
    # path/trigonometry update on both ticks and compensates by halving speed.
    # Preserve the original every-other-tick vehicle cadence on 3DS so the 28
    # traffic actors do half the path-update work at the intended cadence.
    file(READ "${SPAGHETTIKART_WORLD_SOURCE}" SPAGHETTIKART_3DS_WORLD_TEXT)
    set(SPAGHETTIKART_3DS_WORLD_ORIGINAL "${SPAGHETTIKART_3DS_WORLD_TEXT}")
    string(REPLACE
        "#include <memory>"
        "#include <memory>\n#include <cstring>\n#include \"base_actor_pool_3ds.hpp\""
        SPAGHETTIKART_3DS_WORLD_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}"
    )
    if(SPAGHETTIKART_3DS_WORLD_TEXT STREQUAL SPAGHETTIKART_3DS_WORLD_ORIGINAL)
        message(FATAL_ERROR "The upstream World include point changed")
    endif()
    set(SPAGHETTIKART_3DS_WORLD_INCLUDE_TEXT "${SPAGHETTIKART_3DS_WORLD_TEXT}")
    string(REPLACE
        "World* World::Instance;"
        [=[namespace {
constexpr size_t kMk64BaseActorCapacity3DS = 100;
mk64_3ds::BaseActorPool3DS<AActor, kMk64BaseActorCapacity3DS>
    sMk64BaseActors3DS;
}

extern "C" bool Mk64BaseActor3DSIsDynamic(const struct Actor* actor,
                                             size_t permanentCount) {
    if (actor == nullptr) {
        return false;
    }
    const auto* cppActor = reinterpret_cast<const AActor*>(
        reinterpret_cast<const char*>(actor) - sizeof(void*));
    return sMk64BaseActors3DS.IsDynamic(cppActor, permanentCount);
}

World* World::Instance;]=]
        SPAGHETTIKART_3DS_WORLD_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}"
    )
    set(SPAGHETTIKART_3DS_WORLD_POOL_STATE_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}")
    if(SPAGHETTIKART_3DS_WORLD_POOL_STATE_TEXT STREQUAL
       SPAGHETTIKART_3DS_WORLD_INCLUDE_TEXT)
        message(FATAL_ERROR "The upstream World actor-pool state point changed")
    endif()
    string(REPLACE
        [=[struct Actor* World::AddBaseActor() {
    Actors.push_back(std::make_unique<AActor>());

    AActor* actor = Actors.back().get();

    // Skip C++ vtable to access variables in C
    return reinterpret_cast<struct Actor*>(reinterpret_cast<char*>(actor) + sizeof(void*));
}]=]
        [=[struct Actor* World::AddBaseActor() {
    AActor* actor = sMk64BaseActors3DS.FindReusable([](const AActor* candidate) {
        return candidate->Flags == 0 && candidate->Type == 0;
    });
    if (actor != nullptr) {
        actor->bPendingDestroy = false;
        actor->Name = "";
        actor->ResourceName = "";
        actor->Triangles.clear();
        return ConvertAActorToActor(actor);
    }
    if (sMk64BaseActors3DS.Full()) {
        return nullptr;
    }

    Actors.push_back(std::make_unique<AActor>());
    actor = Actors.back().get();
    if (!sMk64BaseActors3DS.Track(actor)) {
        Actors.pop_back();
        return nullptr;
    }
    return ConvertAActorToActor(actor);
}]=]
        SPAGHETTIKART_3DS_WORLD_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}"
    )
    set(SPAGHETTIKART_3DS_WORLD_POOL_ALLOC_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}")
    if(SPAGHETTIKART_3DS_WORLD_POOL_ALLOC_TEXT STREQUAL
       SPAGHETTIKART_3DS_WORLD_POOL_STATE_TEXT)
        message(FATAL_ERROR "The upstream World base-actor allocation point changed")
    endif()
    string(REPLACE
        "    Actors.clear();"
        "    sMk64BaseActors3DS.Reset();\n    Actors.clear();"
        SPAGHETTIKART_3DS_WORLD_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}"
    )
    set(SPAGHETTIKART_3DS_WORLD_POOL_RESET_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}")
    if(SPAGHETTIKART_3DS_WORLD_POOL_RESET_TEXT STREQUAL
       SPAGHETTIKART_3DS_WORLD_POOL_ALLOC_TEXT)
        message(FATAL_ERROR "The upstream World actor cleanup points changed")
    endif()
    string(REPLACE
        [=[void World::TickCameras() {

    for (size_t i = 0; i < 4; i++) {
        ScreenContext* screen = &gScreenContexts[i];
        if (NULL == screen->pendingCamera) { continue; }
        if (screen->pendingCamera != screen->camera) {
            screen->camera = screen->pendingCamera;
            screen->pendingCamera = nullptr;
        }
    }

    for (auto& camera : Cameras) {
        if (camera->IsActive()) {
            camera->Tick();
        }
    }
}]=]
        [=[void World::TickCameras() {

    for (size_t i = 0; i < 4; i++) {
        ScreenContext* screen = &gScreenContexts[i];
        if (NULL == screen->pendingCamera) { continue; }
        if (screen->pendingCamera != screen->camera) {
            screen->camera = screen->pendingCamera;
            screen->pendingCamera = nullptr;
        }
    }

    size_t activeScreens = 1;
    if (gActiveScreenMode == SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL ||
        gActiveScreenMode == SCREEN_MODE_2P_SPLITSCREEN_VERTICAL) {
        activeScreens = 2;
    } else if (gActiveScreenMode == SCREEN_MODE_3P_4P_SPLITSCREEN) {
        activeScreens = 4;
    }
    for (auto& camera : Cameras) {
        if (!camera->IsActive()) {
            continue;
        }
        Camera* rawCamera = camera->Get();
        bool usedByActiveScreen = false;
        for (size_t screenIndex = 0; screenIndex < activeScreens; ++screenIndex) {
            const ScreenContext* screen = &gScreenContexts[screenIndex];
            if (rawCamera == screen->camera || rawCamera == screen->raceCamera ||
                rawCamera == screen->lookBehindCamera) {
                usedByActiveScreen = true;
                break;
            }
        }
        if (usedByActiveScreen) {
            camera->Tick();
        }
    }
}]=]
        SPAGHETTIKART_3DS_WORLD_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}"
    )
    set(SPAGHETTIKART_3DS_WORLD_CAMERA_TICK_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}")
    if(SPAGHETTIKART_3DS_WORLD_CAMERA_TICK_TEXT STREQUAL
       SPAGHETTIKART_3DS_WORLD_POOL_RESET_TEXT)
        message(FATAL_ERROR "The upstream World camera tick loop changed")
    endif()
    string(REPLACE
        [=[void World::TickActors() {
    // This only ticks modded actors
    for (auto& actor : Actors) {
        if (actor->IsMod()) {
            actor->Tick();
        }
    }
}]=]
        [=[void World::TickActors() {
    static bool trafficTickPhase3DS = false;
    trafficTickPhase3DS = !trafficTickPhase3DS;

    // This only ticks modded actors. The four built-in Turnpike traffic
    // classes follow the original game's one-in-two logic cadence.
    for (auto& actor : Actors) {
        if (!actor->IsMod()) {
            continue;
        }
        const char* resourceName = actor->ResourceName;
        if (!trafficTickPhase3DS && gCurrentCourseId == TRACK_TOADS_TURNPIKE &&
            resourceName != nullptr &&
            (std::strcmp(resourceName, "mk:car") == 0 ||
             std::strcmp(resourceName, "mk:bus") == 0 ||
             std::strcmp(resourceName, "mk:truck") == 0 ||
             std::strcmp(resourceName, "mk:tanker_truck") == 0)) {
            continue;
        }
        actor->Tick();
    }
}]=]
        SPAGHETTIKART_3DS_WORLD_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}"
    )
    if(SPAGHETTIKART_3DS_WORLD_TEXT STREQUAL SPAGHETTIKART_3DS_WORLD_CAMERA_TICK_TEXT)
        message(FATAL_ERROR "The upstream World actor tick loop changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_WORLD_SOURCE}" "${SPAGHETTIKART_3DS_WORLD_TEXT}")

