// J.9 - portable, conservative exit-surface validation shared by game and tests.
#pragma once
#include <cmath>
namespace modular {
// Distances are in identical units (game units on the server).
inline bool ValidBallisticExit(float depth, float along, float sideways,
    float outwardFacing, float minimumThickness=0.5f, float maximumSideways=0.75f) {
    return std::isfinite(depth) && std::isfinite(along) &&
        std::isfinite(sideways) && std::isfinite(outwardFacing) &&
        depth > minimumThickness*2 && along > minimumThickness &&
        along < depth-minimumThickness && sideways <= maximumSideways &&
        sideways >= 0.0f && outwardFacing >= 0.15f && outwardFacing <= 1.001f;
}
}
