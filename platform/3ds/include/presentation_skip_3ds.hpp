#pragma once
namespace mk64_3ds {
// A late interpolated tick still renders its keyframe. Only a tick that will
// omit every presentation can omit pure visual preparation.
inline bool SkipVisualPreparation(bool suppress, bool limiter, bool interpolation, bool midpoint) {
    return suppress && !(!limiter && interpolation && midpoint);
}
}
