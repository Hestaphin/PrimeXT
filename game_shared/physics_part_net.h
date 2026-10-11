#pragma once
#include <cstdint>
#include <cmath>
namespace partnet {
constexpr std::uint8_t kVersion = 1;
constexpr std::uint8_t kClear = 1;
constexpr std::uint8_t kDetached = 2;
constexpr int kWireBytes = 2 + 1 + 1 + 4 + 1 + 7 * 4;
constexpr float kActiveInterval = 0.05f;
constexpr float kSleepInterval = 0.5f;
constexpr float kMaxExtrapolationAge = 1.0f;
// Physical-part overrides are time-dependent even if the parent animation is
// stationary. Never reuse cached world bones while any override is active:
// interpolation and stale-snapshot expiry run in StudioApplyPartPoses().
inline bool AnyActivePartPose(const bool *active, int count) {
    if (!active || count <= 0) return false;
    for (int i = 0; i < count; ++i)
        if (active[i]) return true;
    return false;
}

// Receive and render clocks normally share the client-time domain. A clock
// rewind (demo seek/map reset), invalid timestamp or timeout is not a valid
// physical pose; fail back to freshly evaluated Studio animation.
inline bool PartPoseFresh(float now, float received) {
    return std::isfinite(now) && std::isfinite(received) &&
        now >= received && (now - received) <= kMaxExtrapolationAge;
}
// Wrap-safe ordering, assuming fewer than 2^31 updates between packets.
inline bool Newer(std::uint32_t value, std::uint32_t prior) {
    return value != 0 && (prior == 0 ||
        (value != prior && (value - prior) < UINT32_C(0x80000000)));
}
inline bool ValidQuat(float x, float y, float z, float w) {
    const float n=x*x+y*y+z*z+w*w;
    return std::isfinite(n) && n>0.25f && n<4.0f;
}
inline bool ValidPosition(float x,float y,float z) {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}
} // namespace partnet
