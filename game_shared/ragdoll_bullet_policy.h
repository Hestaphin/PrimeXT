#pragma once

// Milestone I.9.4: purpose-specific bullet hit ownership. Never use this
// filter for movement or generic engine traces: a partial ragdoll still needs
// its ordinary GoldSrc movement hull.
namespace ragdollquery
{
inline bool SkipStudioBulletHitbox(bool bulletTrace, bool character,
    bool presentationFlag, bool activePhysicsRagdoll)
{
    return bulletTrace && character && presentationFlag && activePhysicsRagdoll;
}

// Broadphase inputs here are always WORLD axes. The entity's Studio local
// mins/maxs are for client presentation and cannot cull a physics limb.
inline bool OverlapWorldBounds(const float *rayMins, const float *rayMaxs,
    const float *physicsMins, const float *physicsMaxs)
{
    for(int axis = 0; axis < 3; ++axis)
        if(rayMins[axis] > physicsMaxs[axis] || rayMaxs[axis] < physicsMins[axis])
            return false;
    return true;
}
}
