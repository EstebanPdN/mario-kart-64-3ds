#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mk64_3ds::native_sprite {
// Portable premultiplied-alpha bilinear scaler shared with the Python preview.
// Inputs come from the owner's archive; no game artwork lives here.
struct Image {
    unsigned width = 0, height = 0;
    std::vector<uint32_t> pixels;
    bool Valid() const { return width && height && pixels.size() == size_t(width) * height; }
    uint32_t At(int x, int y) const {
        return pixels[size_t(std::clamp(y, 0, int(height) - 1)) * width +
                      std::clamp(x, 0, int(width) - 1)];
    }
};

inline Image ScaleBilinear(const Image& source, unsigned width, unsigned height) {
    // Bound malformed dimensions before multiplication/allocation. All UI
    // callers use at most 320x240; the larger limit permits the font strip.
    if (!source.Valid() || source.width > 512 || source.height > 512 ||
        !width || !height || width > 512 || height > 512) return {};
    Image result{width, height, {}};
    result.pixels.resize(size_t(width) * height);
    for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) {
        const float sx=(x+.5f)*source.width/width-.5f, sy=(y+.5f)*source.height/height-.5f;
        const int ix=std::floor(sx),iy=std::floor(sy);
        const float fx=sx-ix,fy=sy-iy;
        const float weights[4]={(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy};
        const uint32_t samples[4]={source.At(ix,iy),source.At(ix+1,iy),source.At(ix,iy+1),source.At(ix+1,iy+1)};
        float alpha=0,channels[3]{};
        for (unsigned i=0;i<4;++i) {
            const float coverage=weights[i]*(samples[i]>>24);
            alpha+=coverage;
            for (unsigned c=0;c<3;++c) channels[c]+=coverage*((samples[i]>>(c*8))&255);
        }
        uint32_t pixel=uint32_t(std::lround(alpha))<<24;
        if (alpha>0) for (unsigned c=0;c<3;++c)
            pixel|=uint32_t(std::lround(channels[c]/alpha))<<(c*8);
        result.pixels[size_t(y)*width+x]=pixel;
    }
    return result;
}
} // namespace mk64_3ds::native_sprite
