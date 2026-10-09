#pragma once

// I.10.4: common deterministic surface tessellation for PhysX primitive-only
// interactive hinge parts. These vertices are in the body's shape-local space
// (before BodyDef::offset / angles) and are never pre-scaled.
// Both server GoldSrc movement sweeps and client prediction use this geometry.
#include "physics_asset_format.h"
#include "physics_part_layout.h"
#include <array>
#include <cmath>
#include <vector>

namespace physicsasset
{
using PrimitiveTriangle = std::array<Vec3, 3>;
// The template argument must be resolved outside capturing lambdas on MSVC.
inline constexpr int PrimitiveRingSegments = 12;
using PrimitiveRing = std::array<Vec3, PrimitiveRingSegments>;

inline int PrimitiveHingeBodyIndex(const AssetDef &asset, const PartLayout &part)
{
    if(part.bodyIndex < 0 || static_cast<size_t>(part.bodyIndex) >= asset.bodies.size() ||
       !part.collisionIndices.empty()) return -1;
    const BodyDef &body = asset.bodies[part.bodyIndex];
    for(const JointDef &joint : asset.joints)
        if(joint.kind == JointKind::Hinge && SamePartBone(joint.child,body.name))
            return part.bodyIndex;
    return -1;
}

inline void BuildPrimitiveTraceTriangles(const ShapeDef &shape, std::vector<PrimitiveTriangle> &triangles)
{
    const auto emit = [&](const Vec3 &a, const Vec3 &b, const Vec3 &c) {
        triangles.push_back({a,b,c});
    };
    if(shape.type == ShapeType::Box)
    {
        const Vec3 &h = shape.halfExtents;
        if(!(h.x > 0 && h.y > 0 && h.z > 0)) return;
        const std::array<Vec3,8> p = {{{-h.x,-h.y,-h.z},{h.x,-h.y,-h.z},
            {h.x,h.y,-h.z},{-h.x,h.y,-h.z},{-h.x,-h.y,h.z},
            {h.x,-h.y,h.z},{h.x,h.y,h.z},{-h.x,h.y,h.z}}};
        const int faces[6][4] = {{0,3,2,1},{4,5,6,7},{0,1,5,4},
                                 {1,2,6,5},{2,3,7,6},{3,0,4,7}};
        for(const auto &f : faces) {emit(p[f[0]],p[f[1]],p[f[2]]); emit(p[f[0]],p[f[2]],p[f[3]]);}
        return;
    }
    if(!(shape.radius > 0) || !std::isfinite(shape.radius)) return;
    if(shape.type != ShapeType::Sphere && shape.type != ShapeType::Capsule) return;
    const float half = shape.type == ShapeType::Capsule ? shape.halfHeight : 0.0f;
    if(half < 0 || !std::isfinite(half)) return;
    constexpr int segments = PrimitiveRingSegments, hemisphereSteps = 4;
    constexpr float pi = 3.14159265358979323846f;
    const Vec3 left{-half-shape.radius,0,0}, right{half+shape.radius,0,0};
    std::vector<PrimitiveRing> rings;
    const auto ring = [&](float x, float r) {
        PrimitiveRing points{};
        for(int i=0;i<segments;++i)
        {
            const float a = 2*pi*static_cast<float>(i)/segments;
            points[i] = {x,r*std::cos(a),r*std::sin(a)};
        }
        rings.push_back(points);
    };
    for(int k=1;k<=hemisphereSteps;++k)
    {
        const float a=(pi*0.5f)*static_cast<float>(k)/hemisphereSteps;
        ring(-half-shape.radius*std::cos(a),shape.radius*std::sin(a));
    }
    if(half > 0) ring(half,shape.radius);
    for(int k=1;k<hemisphereSteps;++k)
    {
        const float a=(pi*0.5f)*static_cast<float>(k)/hemisphereSteps;
        ring(half+shape.radius*std::sin(a),shape.radius*std::cos(a));
    }
    for(int i=0;i<segments;++i)
    {
        const int next=(i+1)%segments;
        emit(left,rings.front()[next],rings.front()[i]);
        for(size_t r=0;r+1<rings.size();++r)
        {
            emit(rings[r][i],rings[r][next],rings[r+1][next]);
            emit(rings[r][i],rings[r+1][next],rings[r+1][i]);
        }
        emit(rings.back()[i],rings.back()[next],right);
    }
}
}
