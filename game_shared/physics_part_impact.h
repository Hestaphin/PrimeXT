// Milestone I.5 - portable impact-selection rules shared by PhysX game paths.
// No engine or PhysX headers: validated independently of the SDK.
#ifndef PHYSICS_PART_IMPACT_H
#define PHYSICS_PART_IMPACT_H
#include <cmath>

namespace physicsasset
{
inline bool PhysicsPartRayHitWins(float distance, float nearest)
{
    // Existing legacy/world hit wins when effectively tied.
    return std::isfinite(distance) && std::isfinite(nearest) &&
        distance >= 0.0f && distance + 0.001f < nearest;
}

inline float PhysicsPartBlastWeight(float distance, float radius)
{
    if(!std::isfinite(distance) || !std::isfinite(radius) || radius <= 0.001f ||
        distance < 0.0f || distance >= radius) return 0.0f;
    return 1.0f - distance / radius;
}

inline bool PhysicsPartMayReact(bool alreadyFree, bool authoredReactive,
                               bool hasHinge, bool dynamicCompatible, float magnitude)
{
    return (alreadyFree || authoredReactive) && hasHinge && dynamicCompatible &&
        std::isfinite(magnitude) && magnitude > 0.0f;
}
} // namespace physicsasset
#endif
