#pragma once
#include <algorithm>

namespace mk64_3ds {
struct ViewportClipRect { int x, y, width, height; };
// A viewport is a transform, not a raster clip on PICA. Native sky passes
// reset RDP scissor to fullscreen; don't let their oversized quads paint into
// another results window (or the black strip exposed by its departure).
inline ViewportClipRect ClipViewportScissor(int x, int y, int width, int height,
    int sx, int sy, int sw, int sh, bool scissor, int targetWidth, int targetHeight) {
    const int left = std::max({0, x, scissor ? sx : 0});
    const int bottom = std::max({0, y, scissor ? sy : 0});
    const int right = std::min({targetWidth, x + std::max(width, 0),
        scissor ? sx + std::max(sw, 0) : targetWidth});
    const int top = std::min({targetHeight, y + std::max(height, 0),
        scissor ? sy + std::max(sh, 0) : targetHeight});
    return {left, bottom, std::max(0, right-left), std::max(0, top-bottom)};
}
// PICA viewport origins are unsigned. Clamping a negative origin alone pins
// a departing results camera to the edge. Translate homogeneous XY by the
// lost offset; the viewport keeps its size and Z/W remain completely intact.
inline float ViewportSlideOffset(int origin, int extent) {
    return extent > 0 ? 2.0f*std::min(origin, 0)/extent : 0.0f;
}
inline bool ViewportOnscreen(int x, int y, int width, int height,
                            int screenWidth, int screenHeight) {
    return width > 0 && height > 0 && x < screenWidth && y < screenHeight &&
           x+width > 0 && y+height > 0;
}
} // namespace mk64_3ds
