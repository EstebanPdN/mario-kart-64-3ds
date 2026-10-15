#ifndef MK64_THREAT_WARNING_3DS_INC_H
#define MK64_THREAT_WARNING_3DS_INC_H
// Geometry only: no allocations, audio channel reassignment or inventory state.
// AUTO uses the watched racer, not a hardcoded player-zero camera.
// Shared by the sound policy and lower-screen scanner; keep their edges aligned.
static const f32 kMk64ThreatRedRange3DS = 1800.0f;
static const f32 kMk64ThreatStarRange3DS = 450.0f;
static const f32 kMk64ThreatStarHeight3DS = 25.0f;
static bool Mk64ThreatListener3DS(s32 id) {
    return id >= 0 && id < NUM_PLAYERS && !gDemoMode &&
        (gPlayers[id].type & PLAYER_EXISTS) &&
        ((gPlayers[id].type & PLAYER_HUMAN) || Mk64Auto3DSWatchedAudio(id)) &&
        !(gPlayers[id].type & (PLAYER_CINEMATIC_MODE | PLAYER_INVISIBLE_OR_BOMB)) &&
        gLapCountByPlayerId[id] < 3 && !(gPlayers[id].effects & BOO_EFFECT);
}
static bool Mk64ThreatBehind3DS(s32 id, const f32* position, f32 range, f32 height) {
    if (!Mk64ThreatListener3DS(id)) return false;
    Player* listener = &gPlayers[id];
    f32 x = position[0] - listener->pos[0];
    f32 y = position[1] - listener->pos[1];
    f32 z = position[2] - listener->pos[2];
    if ((height > 0 && y*y > height*height) ||
        (range > 0 && x*x + y*y + z*z > range*range)) return false;
    Vec3f forward = {0.0f, 0.0f, 1.0f};
    func_802B64C4(forward, listener->rotation[1] + listener->unk_0C0);
    return x*forward[0] + z*forward[2] < 0.0f;
}
#endif
