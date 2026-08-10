#include <array>
#include <cstddef>
#include <cstdint>

namespace {

constexpr size_t kMaxAudioBlobCount = 1024;

struct AudioBlobRange {
    uintptr_t begin = 0;
    uintptr_t end = 0;
};

std::array<AudioBlobRange, kMaxAudioBlobCount> sAudioBlobs;
size_t sAudioBlobCount = 0;

} // namespace

extern "C" void AudioDma_Register(const void* base, size_t size) {
    const uintptr_t begin = reinterpret_cast<uintptr_t>(base);
    if (begin == 0 || size == 0 || begin + size < begin) {
        return;
    }

    for (size_t i = 0; i < sAudioBlobCount; ++i) {
        if (sAudioBlobs[i].begin == begin && sAudioBlobs[i].end == begin + size) {
            return;
        }
    }

    if (sAudioBlobCount >= sAudioBlobs.size()) {
        return;
    }
    sAudioBlobs[sAudioBlobCount++] = { begin, begin + size };
}

extern "C" size_t AudioDma_Clamp(uintptr_t address, size_t bytes) {
    for (size_t i = 0; i < sAudioBlobCount; ++i) {
        if (address >= sAudioBlobs[i].begin && address < sAudioBlobs[i].end) {
            const size_t available = sAudioBlobs[i].end - address;
            return bytes <= available ? bytes : available;
        }
    }
    return bytes;
}

extern "C" void AudioDma_Clear(void) {
    sAudioBlobCount = 0;
}
