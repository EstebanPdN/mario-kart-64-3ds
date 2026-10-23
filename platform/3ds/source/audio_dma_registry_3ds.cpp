#include "audio_dma_range_cache_3ds.hpp"

namespace {
// Registration and clearing happen at serialized resource boundaries; only
// the synthesis owner queries the table while its job is running.
mk64_3ds::AudioDmaRangeCache sAudioRanges;
}

extern "C" void AudioDma_Register(const void* base, size_t size) {
    sAudioRanges.Register(reinterpret_cast<uintptr_t>(base), size);
}

extern "C" size_t AudioDma_Clamp(uintptr_t address, size_t bytes) {
    return sAudioRanges.Clamp(address, bytes);
}

extern "C" void AudioDma_Clear(void) {
    sAudioRanges.Clear();
}
