// Keep the pinned audio implementation intact, with a narrow entry guard.
#define adsr_update Mk64OriginalAdsrUpdate
#include "../../../third_party/SpaghettiKart/src/audio/effects.c"
#undef adsr_update

unsigned int gMk64AudioMissingEnvelope3DS = 0;

f32 adsr_update(struct AdsrState* adsr) {
    // A missing bank/sequence envelope must silence this voice, not read
    // address zero. The public first-lap dump faults at envelope[envIndex].
    // Count the event once; disabled voices take the original idle path.
    if (adsr->state != ADSR_STATE_DISABLED && adsr->envelope == NULL) {
        __atomic_fetch_add(&gMk64AudioMissingEnvelope3DS, 1U, __ATOMIC_RELAXED);
        adsr->action = 0;
        adsr->state = ADSR_STATE_DISABLED;
        adsr->current = 0.0f;
        return 0.0f;
    }
    return Mk64OriginalAdsrUpdate(adsr);
}
