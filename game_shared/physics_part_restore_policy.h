#pragma once

// I.10.9: SDK-independent admission checks for restoring saved interactive
// parts. Never re-enable a dynamic actor merely because a saved record names it.
// This policy is shared by portable regression tests and the PhysX restore path.
#include <cmath>

namespace physicsasset
{
struct PartRestoreSetup
{
    bool hasActor = false;
    bool dynamicCompatible = false;
    bool jointAttached = false; // live revolute joint, with a valid parent/child pair
    bool ownerAnimated = false;
    bool actorKinematic = false;
    bool alreadyDetached = false;
    bool articulatedOverride = false;
    bool collisionRequested = false;
    bool uniformScale = false;
    float driveStrength = 0.0f;
    float driveDamping = 0.0f;
    float driveMaxForce = 0.0f;
};

// Modes are the stable on-disk values: 0 = Animated, 1 = Driven, 2 = Free.
// A detached snapshot must have been Free when saved. Detachment itself is
// permitted only from an intact, compatible newly reconstructed hinge.
inline bool CanRestoreInteractivePart(const PartRestoreSetup &setup,
    int savedMode, int savedDetached)
{
    if(savedMode < 0 || savedMode > 2 ||
       (savedDetached != 0 && savedDetached != 1) ||
       (savedDetached && savedMode != 2) || !setup.hasActor ||
       setup.articulatedOverride || !setup.ownerAnimated ||
       !setup.actorKinematic || setup.alreadyDetached)
        return false;
    if(savedMode == 0) return true; // keep the newly built animated actor
    if(!setup.dynamicCompatible || !setup.jointAttached ||
       !setup.collisionRequested || !setup.uniformScale)
        return false;
    if(savedMode == 1 &&
       (!std::isfinite(setup.driveStrength) || setup.driveStrength <= 0.0f ||
        !std::isfinite(setup.driveDamping) || setup.driveDamping < 0.0f ||
        !std::isfinite(setup.driveMaxForce) || setup.driveMaxForce < 0.0f))
        return false;
    return true;
}

inline bool ValidRestoredPartSnapshot(float damage, const float (&pose)[7],
    const float (&linear)[3], const float (&angular)[3])
{
    if(!std::isfinite(damage) || damage < 0.0f) return false;
    for(float x : pose) if(!std::isfinite(x)) return false;
    for(float x : linear) if(!std::isfinite(x)) return false;
    for(float x : angular) if(!std::isfinite(x)) return false;
    const double q2 = double(pose[3])*pose[3] + double(pose[4])*pose[4] +
        double(pose[5])*pose[5] + double(pose[6])*pose[6];
    return q2 > 0.98 && q2 < 1.02;
}
}
