// J.10.2 legacy approximation, retained for old portable API/tests only.
// J.10.6.1 runtime uses the exact engine query in ballistic_world_query.h.
// Fixed samples cannot certify narrow gaps; do not use this for penetration.
// The engine exposes the world as one edict; matching edicts in reverse
// traces does NOT prove two surfaces are on the same brush.
#pragma once
#include <cmath>
namespace modular {
struct WorldExitInterval {
    float insideDistance = 0;
    float outsideDistance = 0;
};
// solidAt(distance) samples BSP point contents along the original entry ray.
// Refuse thin/unresolved geometry rather than selecting a later brush's exit.
template <typename SolidAt>
bool FindFirstWorldSolidExit(float maxDistance, SolidAt solidAt, WorldExitInterval &out) {
    constexpr float firstProbe = 0.0625f;
    constexpr float spacing = 0.25f;
    if (!std::isfinite(maxDistance) || maxDistance <= firstProbe + spacing ||
        !solidAt(firstProbe)) return false;
    float inside = firstProbe;
    for (float distance = firstProbe + spacing; distance < maxDistance;
         distance += spacing) {
        if (!solidAt(distance)) {
            float outside = distance;
            for (int n = 0; n < 10; ++n) {
                const float middle = (inside + outside) * 0.5f;
                if (solidAt(middle)) inside = middle;
                else outside = middle;
            }
            if (outside <= 0.5f || outside >= maxDistance - 0.5f) return false;
            out.insideDistance = inside;
            out.outsideDistance = outside;
            return true;
        }
        inside = distance;
    }
    return false;
}
}
