// Accept one collision immediately; orbiting shells cannot restart that hit.
static bool sMk64HitProtected3DS[NUM_PLAYERS];
static f32 sMk64HitNormalSince3DS[NUM_PLAYERS];
static void Mk64ItemHitReset3DS(void) {
    for (s32 i=0;i<NUM_PLAYERS;++i) {
        sMk64HitProtected3DS[i]=false;sMk64HitNormalSince3DS[i]=-1.0f;
    }
}
void Mk64ItemHit3DSTick(void* racer) {
    if (!Mk64NewItems3DSEnabled()) return;
    Player* p=(Player*)racer;s32 id=p-gPlayerOne;
    if (id<0 || id>=NUM_PLAYERS || !(p->type&PLAYER_EXISTS)) return;
    bool hit = Mk64RacerHitMotion3DS(p) ||
        (p->triggers & (HIT_TRIGGERS | HIT_BANANA_TRIGGER | SPINOUT_TRIGGER));
    if (hit) {sMk64HitProtected3DS[id]=true;sMk64HitNormalSince3DS[id]=-1.0f;return;}
    if (!sMk64HitProtected3DS[id]) return;
    // Ordinary hops/drifts after recovery still count as normal driving;
    // repeatedly hopping must not renew the shield indefinitely.
    if (p->lakituProps || p->speed<0.5f ||
        (p->type&(PLAYER_START_SEQUENCE|PLAYER_CINEMATIC_MODE))) {
        sMk64HitNormalSince3DS[id]=-1.0f;return;
    }
    if (sMk64HitNormalSince3DS[id]<0.0f) sMk64HitNormalSince3DS[id]=gCourseTimer;
    else if (gCourseTimer-sMk64HitNormalSince3DS[id]>=2.0f) sMk64HitProtected3DS[id]=false;
}
bool Mk64ItemHit3DSProtected(void* racer) {
    if (!Mk64NewItems3DSEnabled()) return false;
    Player* p=(Player*)racer;s32 id=p-gPlayerOne;
    if (id<0 || id>=NUM_PLAYERS || !(p->type&PLAYER_EXISTS)) return false;
    if (p->effects&(STAR_EFFECT|BOO_EFFECT)) return false;
    Mk64ItemHit3DSTick(p);
    return sMk64HitProtected3DS[id];
}
bool Mk64ItemHit3DSTry(void* racer) {
    if (!Mk64NewItems3DSEnabled()) return true;
    Player* p=(Player*)racer;s32 id=p-gPlayerOne;
    if (id<0 || id>=NUM_PLAYERS || !(p->type&PLAYER_EXISTS)) return false;
    if (p->effects&(STAR_EFFECT|BOO_EFFECT)) return true;
    if (Mk64ItemHit3DSProtected(p)) return false;
    sMk64HitProtected3DS[id]=true;sMk64HitNormalSince3DS[id]=-1.0f;
    return true;
}
