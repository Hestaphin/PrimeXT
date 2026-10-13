// Regression for the coherent *uniform* scaling policy used by dynamic
// .phys ragdolls and interactive hinge actors (not a PhysX runtime test).
#include "physics_asset_scale.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <string>
#include <iostream>

int main()
{
    using namespace physicsasset;
    float scale = -1.f;
    assert(RigidUniformScale({1.f, 1.f, 1.f}, scale) && scale == 1.f);
    assert(RigidUniformScale({2.f, 2.f, 2.f}, scale) && scale == 2.f);
    assert(RigidUniformScale({0.25f, 0.25f, 0.25f}, scale) && scale == 0.25f);
    assert(!RigidUniformScale({2.f, 1.f, 2.f}, scale));
    assert(!RigidUniformScale({-1.f, -1.f, -1.f}, scale));
    assert(!RigidUniformScale({0.f, 0.f, 0.f}, scale));
    assert(!RigidUniformScale({std::numeric_limits<float>::infinity(), 1.f, 1.f}, scale));
    assert(!RigidUniformScale({std::numeric_limits<float>::quiet_NaN(), 1.f, 1.f}, scale));
    const Vec3 bone = ScaleRigidPoint({12.f, -3.f, 5.f}, 2.f);
    assert(bone.x == 24.f && bone.y == -6.f && bone.z == 10.f);
    // Uniform scaling must preserve directions/angles; child-frame local
    // anchor and primitive extents are expressed in the same unit scale.
    const Vec3 pivot = ScaleRigidPoint({-4.f, 7.f, 0.f}, 0.5f);
    assert(pivot.x == -2.f && pivot.y == 3.5f && pivot.z == 0.f);
    AssetDef asset;
    CollisionDef section;
    section.vertices.push_back({3.f, -2.f, 1.f});
    asset.collisions.push_back(section);
    BodyDef body;
    body.offset = {5.f, 1.f, -4.f};
    body.shape.halfExtents = {2.f, 3.f, 4.f};
    body.shape.radius = 6.f;
    body.shape.halfHeight = 7.f;
    body.mass = 13.f;
    asset.bodies.push_back(body);
    JointDef joint;
    joint.hingeAnchor = {-1.f, 0.f, 8.f};
    joint.hingeAxis = {0.f, 1.f, 0.f};
    asset.joints.push_back(joint);
    assert(ScaleAuthoredAsset(asset, 2.f));
    assert(asset.collisions[0].vertices[0].x == 6.f);
    assert(asset.bodies[0].offset.x == 10.f);
    assert(asset.bodies[0].shape.halfExtents.z == 8.f);
    assert(asset.bodies[0].shape.radius == 12.f);
    assert(asset.bodies[0].shape.halfHeight == 14.f);
    assert(asset.bodies[0].mass == 13.f); // mass is an authored parameter
    assert(asset.joints[0].hingeAnchor.z == 16.f);
    assert(asset.joints[0].hingeAxis.y == 1.f); // axis is a DIRECTION
    assert(!ScaleAuthoredAsset(asset, 0.f));
    assert(!ScaleAuthoredAsset(asset, -2.f));
    assert(!ScaleAuthoredAsset(asset, std::numeric_limits<float>::infinity()));
    assert(asset.bodies[0].shape.radius == 12.f);
    assert(!ScaleAuthoredAsset(asset, std::numeric_limits<float>::max()));
    assert(asset.bodies[0].shape.radius == 12.f); // failure is atomic
    std::cout << "Uniform runtime and permanent authoring .phys scale tests PASS\n";
}
