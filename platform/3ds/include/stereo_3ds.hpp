#pragma once

// Top-screen stereo: the per-draw clip-space shift and depth-driven
// auto-convergence, ported from rt64-3ds (src/renderer/rt64_3ds_stereo.*).
// The vertex shader applies x' = x + eye * (a * w + b), eye +1 left, -1 right,
// so a draw's commands are the same for both eyes and the right eye can run
// the left eye's command list again.
//
// `sep` is the per-eye disparity at infinity as a fraction of the half screen
// width (NDC): full on-screen disparity = sep * screen width. It does not
// depend on the camera's field of view, and convergence does not change it.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace mk64_3ds {

// Settings -> frame values. The defaults live in settings_3ds.cpp.
constexpr float kStereoSeparationUnit = 0.002f;
constexpr float kStereoPanelGain = 0.5f;           // the desktop range on a 3DS panel
constexpr float kStereoConvergenceUnit = 2.0f;     // world units per convergence_tenths step
constexpr float kStereoPxPerNdc = 200.0f;          // half of the 400-wide top screen

// rt64-3ds's orthographic HUD terms: the tuning anchor is separation 0.10.
constexpr float kStereoHudMaxOffset = 0.04f;
constexpr float kStereoHudReferenceSeparation = 0.10f;
constexpr float kStereoHudOrthoFactor = 2.75f;

// A G_NOOP with this parameter is a stereo layer marker from the game's
// display list: its length field is non-zero while sky actors are drawn.
constexpr std::uint32_t kStereoSkyLayerNoop = 0x40;

enum class StereoClass : std::uint8_t {
    Screen,    // no shift: fades and flat fills, whose edges must not show
    World,     // zero parallax at the convergence distance, full at infinity
    Infinity,  // sky actors: the full separation at any depth
    Hud,       // the HUD depth setting
};

struct StereoFrame {
    bool on = false;
    float sep = 0.0f;
    float conv = 0.0f;
    float hudNdc = 0.0f;   // the left eye's HUD shift, on whole screen pixels
};

// Slider and setting to the per-eye separation.
constexpr float StereoSeparation(int setting, float slider) {
    return static_cast<float>(setting) * kStereoSeparationUnit * slider * kStereoPanelGain;
}

constexpr float StereoConvergence(int tenths) {
    return static_cast<float>(tenths) * kStereoConvergenceUnit;
}

// Pop-out is not divergence: behind the screen the limit is physical, in front
// of it only comfort. Positive ndc here means nearer than the screen, for the
// left eye (shifted right, crossed).
inline float StereoHudShift(int hudDepth, float sep) {
    const float centered = (static_cast<float>(hudDepth) - 50.0f) / 50.0f;
    const float wanted = centered * kStereoHudMaxOffset * kStereoHudOrthoFactor *
                         (sep / kStereoHudReferenceSeparation);
    const float limit = wanted < 0.0f ? 0.10f : 0.30f;
    const float ndc = std::clamp(wanted, -limit, limit);
    // Text stays identical in both eyes on whole pixels; roundf is symmetric.
    return std::round(ndc * kStereoPxPerNdc) / kStereoPxPerNdc;
}

// The left eye's (a, b). World: x += sep * (conv - w), nearer than convergence
// moves right in the left eye (crossed). The other classes draw at w = 1.
inline void StereoShift(const StereoFrame& frame, StereoClass cls, float* a, float* b) {
    *a = 0.0f;
    *b = 0.0f;
    if (!frame.on) return;
    switch (cls) {
        case StereoClass::World:
            *a = -frame.sep;
            *b = frame.sep * frame.conv;
            break;
        case StereoClass::Infinity:
            *b = -frame.sep;
            break;
        case StereoClass::Hud:
            *b = frame.hudNdc;
            break;
        case StereoClass::Screen:
        default:
            break;
    }
}

// Tuning for a chase camera at about 30 updates a second.
struct StereoAutoConvergenceTuning {
    float regionBottom = 0.85f;  // the near statistic reads the frame down to here: the kart, not the road under the camera
    int history = 9;             // frames in the median filter
    float approach = 0.25f;      // per-update pull toward a nearer depth
    float retreat = 0.05f;       // ... and toward a farther one
    float cutRatio = 2.5f;       // this much nearer for 2 updates is a camera cut: jump there
    float easeIn = 0.14f;        // convergence easing toward the target, nearer
    float easeOut = 0.06f;       // ... and farther
    float nearClamp = 5.0f;      // convergence never comes nearer than this (world units)
};

// Positive view distance of a reversed-depth sample z' (= (ndc z + 1) / 2)
// under guPerspective's m[2][2] and m[3][2]; <= 0 when invalid.
inline float StereoViewDepth(float zp, float m22, float m32) {
    if (!(zp > 0.0f) || !(zp < 1.0f)) return -1.0f;
    const float ndc = 2.0f * zp - 1.0f;
    const float denominator = -ndc - m22;
    if (std::fabs(denominator) < 1e-6f) return -1.0f;
    const float z = m32 / denominator;
    return z > 0.0f ? z : -z;
}

// Keeps the nearest significant object's pop-out within the comfort budget by
// pulling convergence in, never beyond the manual value. Smoothing runs on
// 1 / convergence because disparity is linear in it. A frame without a near
// depth keeps the last convergence: a sky with a few objects in it must not
// flap between that and the manual value.
struct StereoAutoConvergence {
    static constexpr int kHistory = 9;
    float history[kHistory] = {};
    int count = 0;
    int cursor = 0;
    float zEma = -1.0f;
    float invConv = -1.0f;
    int cutFrames = 0;
    float lastConv = -1.0f;

    void Reset() {
        count = cursor = cutFrames = 0;
        zEma = invConv = lastConv = -1.0f;
    }

    float Update(float nearestZ, float manualConv, float sep, int comfortThousandths,
                 const StereoAutoConvergenceTuning& tune) {
        if (sep <= 0.0f || manualConv <= 0.0f) return manualConv;
        if (nearestZ <= 0.0f) return lastConv > 0.0f ? std::min(lastConv, manualConv) : manualConv;

        const int length = std::max(1, std::min(kHistory, tune.history));
        history[cursor] = nearestZ;
        cursor = (cursor + 1) % kHistory;
        count = count < length ? count + 1 : length;
        float sorted[kHistory];
        for (int i = 0; i < count; ++i) sorted[i] = history[(cursor + kHistory - 1 - i) % kHistory];
        std::sort(sorted, sorted + count);
        const float zMedian = sorted[count / 2];

        if (zEma <= 0.0f) {
            zEma = zMedian;
        } else {
            const float relative = std::fabs(zMedian - zEma) / zEma;
            // A ratio on the raw sample, approach only: |z - ema| / ema can
            // never reach 1 on approach, and a symmetric test thrashes on
            // anything moving close to the camera.
            const float approach = nearestZ < zEma ? zEma / nearestZ : 1.0f;
            cutFrames = approach > tune.cutRatio ? cutFrames + 1 : 0;
            if (cutFrames >= 2 && count >= 3) {
                float recent[3];
                for (int i = 0; i < 3; ++i) recent[i] = history[(cursor + kHistory - 1 - i) % kHistory];
                std::sort(recent, recent + 3);
                zEma = recent[1];
                invConv = -1.0f;
                cutFrames = 0;
            } else if (relative > 0.02f) {
                zEma += (zMedian - zEma) * (zMedian < zEma ? tune.approach : tune.retreat);
            }
        }

        const float target = static_cast<float>(std::clamp(comfortThousandths, -50, 60)) * 0.001f;
        const float atManual = sep * 0.5f * (manualConv / zEma - 1.0f);
        float conv = manualConv;
        if (atManual > target) conv = std::min(zEma * (1.0f + 2.0f * target / sep), manualConv);
        conv = std::max(conv, zEma * 0.25f);
        conv = std::max(conv, tune.nearClamp);

        const float targetInverse = 1.0f / conv;
        if (invConv <= 0.0f) invConv = targetInverse;
        else invConv += (targetInverse - invConv) * (targetInverse > invConv ? tune.easeIn : tune.easeOut);
        lastConv = std::min(1.0f / invConv, manualConv);
        return lastConv;
    }
};

struct StereoDepthSample {
    float x;   // screen fraction, 0 left .. 1 right
    float y;   // screen fraction, 0 top .. 1 bottom
    float zp;  // (ndc z + 1) / 2; only 0 < zp < 1 is depth
};

// The near statistic: a 9x5 patch layout inside the frame minus its margins,
// the 25th percentile within each patch so one stray texel cannot pull it, and
// then the second nearest patch so one small object is not the whole story.
// Returns z', or -1 when fewer than 3 patches have depth.
inline float StereoNearDepth(const StereoDepthSample* samples, std::size_t count, float regionBottom) {
    constexpr int kCols = 9, kRows = 5, kPatchSamples = 32;
    static float patch[kCols * kRows][kPatchSamples];
    int filled[kCols * kRows] = {};
    constexpr float x0 = 0.05f, x1 = 0.95f, y0 = 0.05f;
    const float y1 = std::clamp(regionBottom, 0.3f, 1.0f);
    for (std::size_t i = 0; i < count; ++i) {
        const StereoDepthSample& sample = samples[i];
        if (!(sample.zp > 0.0f) || !(sample.zp < 1.0f)) continue;
        if (sample.x < x0 || sample.x >= x1 || sample.y < y0 || sample.y >= y1) continue;
        const int col = std::min(kCols - 1, static_cast<int>((sample.x - x0) / (x1 - x0) * kCols));
        const int row = std::min(kRows - 1, static_cast<int>((sample.y - y0) / (y1 - y0) * kRows));
        int& n = filled[row * kCols + col];
        if (n < kPatchSamples) patch[row * kCols + col][n++] = sample.zp;
    }
    float nears[kCols * kRows];
    int n = 0;
    for (int i = 0; i < kCols * kRows; ++i) {
        const int m = filled[i];
        if (m < 4) continue;
        std::nth_element(patch[i], patch[i] + m / 4, patch[i] + m);
        nears[n++] = patch[i][m / 4];
    }
    if (n < 3) return -1.0f;
    const int k = n >= 4 ? 1 : 0;
    std::nth_element(nears, nears + k, nears + n);
    return nears[k];
}

} // namespace mk64_3ds
