#pragma once
// Milestone I.6: portable, bounded revolute joint velocity-servo policy.
// Inputs and outputs use radians and radians/second. A PhysX joint motor
// supplies the reaction torque, limited by maxForce (engine force units).
#include <cmath>
#include <algorithm>
namespace physicsasset {
struct HingeDriveCommand { float velocity = 0.0f; float forceLimit = 0.0f; bool valid = false; };
inline float WrapHingeAngle(float a) {
    if (!std::isfinite(a)) return 0.0f;
    return std::atan2(std::sin(a), std::cos(a));
}
inline float HingeTargetRate(float target, float previous, float seconds) {
    if (!std::isfinite(target) || !std::isfinite(previous) || !std::isfinite(seconds) ||
        seconds < 0.0001f || seconds > 0.25f) return 0.0f;
    return std::clamp(WrapHingeAngle(target - previous) / seconds, -20.0f, 20.0f);
}
inline HingeDriveCommand ComputeHingeDrive(float target, float angle, float targetRate,
    float currentRate, float strength, float damping, float maxForce) {
    HingeDriveCommand out;
    const float values[] = {target, angle, targetRate, currentRate, strength, damping, maxForce};
    for (float v : values) if (!std::isfinite(v)) return out;
    if (strength <= 0.0f || damping < 0.0f || maxForce <= 0.0f) return out;
    // PD velocity-servo. Motor drive force limit bounds actual joint torque.
    const float error = WrapHingeAngle(target - angle);
    const double rate = static_cast<double>(targetRate) +
        static_cast<double>(strength) * error +
        static_cast<double>(damping) * (targetRate - currentRate);
    if (!std::isfinite(rate)) return out;
    out.velocity = static_cast<float>(std::clamp(rate, -20.0, 20.0));
    out.forceLimit = std::clamp(maxForce, 0.0f, 1000000.0f);
    out.valid = true;
    return out;
}
} // namespace physicsasset
