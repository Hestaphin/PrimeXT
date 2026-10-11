#pragma once

// Rigid PhysX primitives and revolute joints cannot express arbitrary
// anisotropic transforms. Support positive UNIFORM entity scale consistently
// and reject non-uniform/mirrored scales until cooked scaled geometry and
// joint frames can be implemented together.
#include "physics_asset_format.h"
#include <cmath>
#include <algorithm>
#include <utility>

namespace physicsasset
{
inline bool RigidUniformScale(const Vec3 &scale, float &uniform)
{
    if(!std::isfinite(scale.x) || !std::isfinite(scale.y) || !std::isfinite(scale.z) ||
       scale.x <= 0.0f || scale.y <= 0.0f || scale.z <= 0.0f)
        return false;
    const float tolerance = 0.0001f * std::max(1.0f, std::max(scale.x, std::max(scale.y, scale.z)));
    if(std::fabs(scale.x - scale.y) > tolerance || std::fabs(scale.x - scale.z) > tolerance)
        return false;
    uniform = scale.x;
    return true;
}

inline Vec3 ScaleRigidPoint(const Vec3 &point, float scale)
{
    return { point.x * scale, point.y * scale, point.z * scale };
}

// Authoring-only operation: permanently resize a .phys file to match a
// Studio model that has itself been rescaled/re-exported. Do NOT call for an
// entity's runtime scale: runtime scales these values exactly once.
// Returns false without changing the asset for nonpositive/overflowing input.
inline bool ScaleAuthoredAsset(AssetDef &asset, float factor)
{
    if(!std::isfinite(factor) || factor <= 0.0f) return false;
    AssetDef resized = asset;
    auto scaleVec = [factor](Vec3 &value)
    {
        value = ScaleRigidPoint(value, factor);
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    for(auto &section : resized.collisions)
        for(auto &vertex : section.vertices)
            if(!scaleVec(vertex)) return false;
    for(auto &body : resized.bodies)
    {
        if(!scaleVec(body.offset) || !scaleVec(body.shape.halfExtents)) return false;
        body.shape.radius *= factor;
        body.shape.halfHeight *= factor;
        if(!std::isfinite(body.shape.radius) || !std::isfinite(body.shape.halfHeight)) return false;
    }
    for(auto &joint : resized.joints)
        if(!scaleVec(joint.hingeAnchor)) return false;
    asset = std::move(resized);
    return true;
}
}
