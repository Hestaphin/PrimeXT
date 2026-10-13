/*
 * Milestone I.9.2: independent root collision ownership policy.
 * Keep this separate from PhysX types so the state transitions can be tested
 * without the optional third-party SDK.
 */
#pragma once

namespace physicsasset
{
enum class RootCollisionAction
{
    Preserve, // no new request; keep existing (e.g. saved legacy) flags
    Enable,
    Disable
};

inline RootCollisionAction ResolveRootCollision(
    bool collisionRequested, bool selectedGeometry,
    bool articulatedOwner, bool ragdollRestorePending,
    bool enableWhenAllowed )
{
    if( !collisionRequested || !selectedGeometry || articulatedOwner || ragdollRestorePending )
        return RootCollisionAction::Disable;
    return enableWhenAllowed ? RootCollisionAction::Enable : RootCollisionAction::Preserve;
}
}
