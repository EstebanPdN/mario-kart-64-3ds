// Race box-seeking: steer to a real box, never enlarge its collision or
// hand out items. Fixed storage, rebuilt once per race after actors have spawned.
typedef struct { size_t actor; s16 waypoint; f32 factor, lane; } Mk64CpuBoxAnchor3DS;
static Mk64CpuBoxAnchor3DS sMk64BoxAnchors3DS[64];
static s32 sMk64BoxAnchorCount3DS;
static bool sMk64BoxesBuilt3DS;
static f32 sMk64BoxLane3DS[NUM_PLAYERS];
static bool sMk64BoxSeeking3DS[NUM_PLAYERS];
static size_t sMk64BoxTarget3DS[NUM_PLAYERS];
static void Mk64CpuBoxSeekReset3DS(void) {
    sMk64BoxesBuilt3DS = false;
    sMk64BoxAnchorCount3DS = 0;
    for (s32 id = 0; id < NUM_PLAYERS; ++id) {
        sMk64BoxLane3DS[id] = 0.0f;
        sMk64BoxSeeking3DS[id] = false;
        sMk64BoxTarget3DS[id] = (size_t)-1;
    }
}
static void Mk64CpuBuildBoxAnchors3DS(void) {
    s32 count = gPathCountByPathIndex[0];
    if (count < 3 || !gTrackPaths[0] || !gTrackLeftPaths[0] || !gTrackRightPaths[0]) return;
    sMk64BoxesBuilt3DS = true;
    for (size_t index = 0; index < CM_GetActorSize() && sMk64BoxAnchorCount3DS < 64; ++index) {
        struct Actor* actor = CM_GetActor(index);
        if (!actor || actor->type != ACTOR_ITEM_BOX || !(actor->flags & 0x8000)) continue;
        s32 nearest = 0;
        f32 best = 1.0e20f;
        for (s32 point = 0; point < count; ++point) {
            TrackPathPoint* p = &gTrackPaths[0][point];
            f32 x = actor->pos[0] - p->x, y = actor->pos[1] - p->y, z = actor->pos[2] - p->z;
            f32 distance = x*x + y*y + z*z;
            if (distance < best) { best = distance; nearest = point; }
        }
        TrackPathPoint* left = &gTrackLeftPaths[0][nearest];
        TrackPathPoint* right = &gTrackRightPaths[0][nearest];
        f32 x = right->x - left->x, z = right->z - left->z;
        f32 width2 = x*x + z*z;
        if (width2 < 1.0f) continue;
        f32 factor = 2.0f * ((actor->pos[0] - left->x)*x + (actor->pos[2] - left->z)*z) / width2 - 1.0f;
        // +/-1 is only the original narrow AI lane (half-width 50), NOT the
        // roadway boundary. All 16 real roadside boxes lie at -1.2 .. -2.1.
        // Follow those actual spawn positions, with a bounded safety envelope;
        // do not broaden AI lanes globally or target a different vertical level.
        const f32 limit = IsToadsTurnpike() ? 2.4f : 1.4f;
        if (factor < -limit || factor > limit || fabsf(actor->pos[1] - gTrackPaths[0][nearest].y) > 45.0f) continue;
        Mk64CpuBoxAnchor3DS* out = &sMk64BoxAnchors3DS[sMk64BoxAnchorCount3DS++];
        out->actor = index; out->waypoint = nearest; out->factor = factor;
        const f32 separation = CM_GetProps()->AIMinimumSeparation;
        out->lane = factor < -separation*0.5f ? -separation : factor > separation*0.5f ? separation : 0.0f;
    }
}
float Mk64CpuSeekBox3DS(int id, int nearest, float nativeFactor) {
    if (!Mk64NewItems3DSEnabled() || IsYoshiValley() || IsPodiumCeremony() ||
        (gModeSelection != GRAND_PRIX && gModeSelection != VERSUS) || id < 0 || id >= NUM_PLAYERS ||
        gPathIndexByPlayerId[id] != 0) return nativeFactor;
    Player* player = &gPlayers[id];
    s32 count = gPathCountByPathIndex[0];
    if (count < 3 || nearest < 0 || nearest >= count) return nativeFactor;
    if (Mk64CpuTraffic3DSAvoiding(id)) {
        // A stale box-approach blend must not override the obstacle lane.
        sMk64BoxSeeking3DS[id]=false;
        sMk64BoxTarget3DS[id]=(size_t)-1;
        sMk64BoxLane3DS[id]=nativeFactor;
        return nativeFactor;
    }
    if (!sMk64BoxesBuilt3DS) Mk64CpuBuildBoxAnchors3DS();
    bool eligible = (player->type & (PLAYER_EXISTS | PLAYER_CPU)) == (PLAYER_EXISTS | PLAYER_CPU) &&
        !(player->type & (PLAYER_HUMAN | PLAYER_START_SEQUENCE | PLAYER_CINEMATIC_MODE | PLAYER_INVISIBLE_OR_BOMB)) &&
        !Mk64RacerHitMotion3DS(player) && !player->lakituProps && !bStopAICrossing[id] &&
        !Mk64CpuTraffic3DSAvoiding(id) &&
        gLapCountByPlayerId[id] < 3 && cpu_ItemStrategy[id].branch == CPU_STRATEGY_WAIT_NEXT_ITEM &&
        sMk64CpuItemReady3DS[id] < 0.0f;
    // Pending roulette is declared in the including translation unit.
    f32 goal = nativeFactor, bestScore = 1.0e20f;
    bool found = false;
    size_t target = (size_t)-1;
    if (eligible) for (s32 i = 0; i < sMk64BoxAnchorCount3DS; ++i) {
        Mk64CpuBoxAnchor3DS* box = &sMk64BoxAnchors3DS[i];
        if (box->actor >= CM_GetActorSize()) continue;
        struct Actor* actor = CM_GetActor(box->actor);
        if (!actor || actor->type != ACTOR_ITEM_BOX || !(actor->flags & 0x8000) || actor->state != 2) continue;
        s32 ahead = (box->waypoint - nearest + count) % count;
        const bool turnpike = IsToadsTurnpike();
        if (ahead > (turnpike ? 26 : 40) ||
            fabsf(actor->pos[1] - gTrackPaths[0][box->waypoint].y) > 45.0f) continue;
        f32 score = ahead + fabsf(box->factor - gTrackPositionFactor[id]) * 5.0f;
        // Spread the row across available boxes instead of all eight cars
        // repeatedly steering at the same box. Reservation never grants items.
        if (sMk64BoxTarget3DS[id] == box->actor) score -= 6.0f;
        for (s32 other=0; other<NUM_PLAYERS; ++other)
            if (other != id && sMk64BoxSeeking3DS[other] && sMk64BoxTarget3DS[other] == box->actor)
                score += 10.0f;
        if (score < bestScore) {
            bestScore = score;
            f32 blend = turnpike ? (26.0f-ahead)/18.0f : (40.0f-ahead)/24.0f;
            if (blend > 1.0f) blend = 1.0f;
            blend = blend*blend*(3.0f-2.0f*blend);
            f32 aiming = box->factor;
            if (!turnpike) {
                // Use the native left/centre/right lane first. Refine only
                // close to the box so its real collision volume can be reached.
                f32 close = (16.0f-ahead)/8.0f;
                if (close < 0.0f) close=0.0f;
                if (close > 1.0f) close=1.0f;
                close=close*close*(3.0f-2.0f*close);
                aiming=box->lane+(box->factor-box->lane)*close;
            }
            goal = nativeFactor + (aiming-nativeFactor)*blend;
            target = box->actor;
            found = true;
        }
    }
    sMk64BoxTarget3DS[id] = target;
    if (!sMk64BoxSeeking3DS[id]) sMk64BoxLane3DS[id] = nativeFactor;
    if (found || sMk64BoxSeeking3DS[id]) {
        f32 delta = goal - sMk64BoxLane3DS[id];
        if (delta > 0.05f) delta = 0.05f;
        if (delta < -0.05f) delta = -0.05f;
        sMk64BoxLane3DS[id] += delta;
        sMk64BoxSeeking3DS[id] = found || fabsf(sMk64BoxLane3DS[id] - nativeFactor) > 0.001f;
        return sMk64BoxLane3DS[id];
    }
    return nativeFactor;
}
