#pragma once

// I.10.10: Movement sweep ownership is independent for the entity/root
// PhysX actor and its animated/hinged child shapes. A shape-less or disabled
// root must not suppress an otherwise active child collision mesh.
namespace physicsasset
{
struct MovementSweepOwners
{
    bool root = false;
    bool children = false;

    bool Any() const { return root || children; }
};

inline MovementSweepOwners SelectMovementSweepOwners(
    bool rootActorExists, bool rootHasShapes, bool rootCollisionEnabled,
    bool activeChildShapes)
{
    if (!rootActorExists) return {};
    return {rootHasShapes && rootCollisionEnabled, activeChildShapes};
}
}
