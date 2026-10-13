#pragma once

#include <cmath>
#include <algorithm>

namespace physicsasset {
// SDK-independent, deterministic single-event damage/impulse policy.
// Impact impulse and damage are different units. A zero threshold disables
// that trigger. Damage accumulates per part and is saturated to avoid overflow.
struct PartFailurePolicy {
    float releaseDamage = 0.0f;
    float breakDamage = 0.0f;
    float breakImpulse = 0.0f;
};
struct PartFailureResult {
    float accumulatedDamage = 0.0f;
    bool release = false;
    bool detach = false;
    bool valid = false;
};
inline PartFailureResult EvaluatePartFailure(const PartFailurePolicy &policy,
    float previousDamage, float damage, float impulse, bool alreadyDetached) {
    PartFailureResult result;
    if (!std::isfinite(previousDamage) || previousDamage < 0.0f ||
        !std::isfinite(damage) || damage < 0.0f ||
        !std::isfinite(impulse) || impulse < 0.0f ||
        !std::isfinite(policy.releaseDamage) || policy.releaseDamage < 0.0f ||
        !std::isfinite(policy.breakDamage) || policy.breakDamage < 0.0f ||
        !std::isfinite(policy.breakImpulse) || policy.breakImpulse < 0.0f)
        return result;
    result.valid = true;
    result.accumulatedDamage = std::min(1000000000.0f, previousDamage +
        std::min(damage, 1000000000.0f));
    // A detached part cannot release or break a joint a second time.
    if (alreadyDetached) return result;
    result.detach = (policy.breakDamage > 0.0f && result.accumulatedDamage >= policy.breakDamage) ||
        (policy.breakImpulse > 0.0f && impulse >= policy.breakImpulse);
    result.release = result.detach ||
        (policy.releaseDamage > 0.0f && result.accumulatedDamage >= policy.releaseDamage);
    return result;
}
} // namespace physicsasset
