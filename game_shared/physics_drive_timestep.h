#pragma once
#include <cmath>

namespace physicsasset
{
struct StepDriveGains { float spring = 0, damping = 0; };
inline StepDriveGains ResolveStepDriveGains(float spring, float damping, float dt,
    float inverseMass = 1.0f)
{
    if(!std::isfinite(spring) || spring < 0 || !std::isfinite(damping) || damping < 0 ||
        !std::isfinite(dt) || dt <= 0 || !std::isfinite(inverseMass) || inverseMass < 0) return {};
    // Retain the legacy 100 Hz drive response. Longer steps use backward-Euler
    // PD gains, preventing high damping from reversing velocity every step.
    if(dt <= 0.010001f) return {spring, damping};
    const double denominator = 1.0 + double(inverseMass) *
        (double(damping) * dt + double(spring) * dt * dt);
    return {float(spring / denominator), float((damping + double(spring) * dt) / denominator)};
}
}
