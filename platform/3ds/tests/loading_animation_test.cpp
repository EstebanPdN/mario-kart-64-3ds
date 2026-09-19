#include "loading_animation_3ds.hpp"
#include "o2r_archive_reader.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
int main(int argc, char** argv) {
    using namespace mk64_3ds;
    assert(argc == 2);
    O2rArchiveReader archive(argv[1]); assert(archive.Open() == O2rReadResult::Ok);
    LoadingAnimation animation;
    assert(LoadAnimationFromArchive(animation, [&](const char* name, std::vector<uint8_t>& bytes) {
        return archive.ReadEntry(name, &bytes) == O2rReadResult::Ok;
    }));
    FILE* cache = std::tmpfile(); assert(cache);
    assert(WriteLoadingCache(cache, 1234, 5678, animation));
    LoadingAnimation copy;
    std::rewind(cache); assert(ReadLoadingCache(cache, 1234, 5678, copy));
    assert(std::memcmp(&copy, &animation, sizeof(copy)) == 0);
    std::rewind(cache); assert(!ReadLoadingCache(cache, 1235, 5678, copy));
    std::rewind(cache); assert(!ReadLoadingCache(cache, 1234, 5679, copy));
    std::fseek(cache, 60, SEEK_SET); int byte = std::fgetc(cache);
    std::fseek(cache, 60, SEEK_SET); std::fputc(byte^1, cache);
    std::rewind(cache); assert(!ReadLoadingCache(cache, 1234, 5678, copy)); std::fclose(cache);
    std::vector<uint8_t> bad(79); assert(!LoadingTexture(bad, 4, 72, 56, copy.pixels.data(), 4032));
    for (unsigned width : {400u, 800u}) {
        std::vector<uint8_t> screen(width * 240 * 3), first;
        for (unsigned f = 0; f < 32; ++f) {
            DrawLoadingAnimation(animation, f * 30, screen.data(), width, 240);
            if (!f) first = screen;
            for (unsigned x = 0; x < width; ++x) for (unsigned y = 0; y < 240; ++y) {
                const auto at = (x*240+239-y)*3;
                if (x < (width-72*(width/400))/2 || x >= (width+72*(width/400))/2 || y < 92 || y >= 148)
                    assert(screen[at] == 0 && screen[at+1] == 0 && screen[at+2] == 0);
                if (width == 800 && x%2) assert(std::memcmp(&screen[at], &screen[at-240*3], 3) == 0);
            }
        }
        DrawLoadingAnimation(animation, 960, screen.data(), width, 240); assert(screen == first);
    }
    std::puts("PASS: all 32 native frames, 30 ms loop, centered 400/800 output, black surroundings, cache roundtrip/staleness/corruption and malformed texture.");
}
