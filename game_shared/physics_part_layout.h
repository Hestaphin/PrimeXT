#pragma once

// Milestone I.2: engine-independent articulated-prop layout.
// One Studio bone owns one physics part, even if several collision sections
// (including bodygroup variants) contribute shapes to that rigid part.
// No PhysX actors are created here. Runtime ownership is assigned by PhysX.
#include "physics_asset_format.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace physicsasset
{
enum class PartOwner { AnimatedProxy, ArticulatedBody };

struct PartLayout
{
    std::string bone;
    std::string name;
    int bodyIndex = -1; // -1: v3 collision-only part, no authored articulated body
    PropDriveMode initialMode = PropDriveMode::Animated;
    std::vector<size_t> collisionIndices; // shapes/variants on one bone
};

inline bool SamePartBone(const std::string &a, const std::string &b)
{
    if(a.size() != b.size()) return false;
    for(size_t i = 0; i < a.size(); ++i)
        if(std::tolower(static_cast<unsigned char>(a[i])) !=
           std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
}

inline std::vector<PartLayout> BuildPartLayout(const AssetDef &asset)
{
    std::vector<PartLayout> result;
    // Collision-first: old v3 collision-only assets retain all moving shapes.
    for(size_t i = 0; i < asset.collisions.size(); ++i)
    {
        const CollisionDef &collision = asset.collisions[i];
        if(collision.bone.empty()) continue; // entity-space chassis, not a part
        auto it = std::find_if(result.begin(), result.end(), [&](const PartLayout &part) {
            return SamePartBone(part.bone, collision.bone);
        });
        if(it == result.end())
        {
            result.push_back(PartLayout{});
            it = result.end() - 1;
            it->bone = collision.bone;
            it->name = collision.bone; // collision-only fallback identifier
        }
        it->collisionIndices.push_back(i);
    }
    // Only explicitly opted-in v4 bodies become interactive prop parts.
    // Plain v1-v4 ragdoll bodies must not silently acquire extra proxies.
    for(size_t i = 0; i < asset.bodies.size(); ++i)
    {
        const BodyDef &body = asset.bodies[i];
        if(body.propDrive == PropDriveMode::None && body.part.empty()) continue;
        auto it = std::find_if(result.begin(), result.end(), [&](const PartLayout &part) {
            return SamePartBone(part.bone, body.bone);
        });
        if(it == result.end())
        {
            result.push_back(PartLayout{});
            it = result.end() - 1;
            it->bone = body.bone;
        }
        it->bodyIndex = static_cast<int>(i);
        it->name = body.part.empty() ? body.name : body.part;
        it->initialMode = body.propDrive == PropDriveMode::None ? PropDriveMode::Animated : body.propDrive;
    }
    return result;
}
}
