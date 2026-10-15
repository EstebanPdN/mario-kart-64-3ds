// Observed CPU roulette uses the original item-window sound scripts, but only
// viewport-zero's native audio slot. Eight racer IDs must never become listener
// IDs. No expansion/reassignment of the fixed M64 playback channels.
static int sMk64AutoAudioPlayer3DS=-1;
static bool sMk64AutoRollingSound3DS=false;
static void Mk64AutoAudio3DSReset() {
    if (sMk64AutoRollingSound3DS) func_800C9018(0,0x0100FE1CU);
    sMk64AutoRollingSound3DS=false;
    sMk64AutoAudioPlayer3DS=-1;
}
static void Mk64AutoAudio3DSTick() {
    if (!Mk64Auto3DSRaceInputLocked() || gIsGamePaused) {
        Mk64AutoAudio3DSReset();
        return;
    }
    const int watched=Mk64Auto3DSObservedPlayer();
    if (sMk64AutoAudioPlayer3DS != watched) Mk64AutoAudio3DSReset();
    sMk64AutoAudioPlayer3DS=watched;
    const bool rolling=Mk64CpuItem3DSRolling(watched);
    if (rolling && !sMk64AutoRollingSound3DS) func_800C8F80(0,0x0100FE1CU);
    if (!rolling && sMk64AutoRollingSound3DS) {
        func_800C9018(0,0x0100FE1CU);
        if (Mk64CpuItem3DSHud(watched) != 0) func_800C8F80(0,0x0100FE47U);
    }
    sMk64AutoRollingSound3DS=rolling;
}
