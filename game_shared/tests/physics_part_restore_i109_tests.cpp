// I.10.9: no PhysX dependency; exercises the policy used by real RestoreInteractiveParts.
#include "physics_part_restore_policy.h"
#include "physics_asset_format.h"
#include <cassert>
#include <limits>
#include <iostream>

int main()
{
    using namespace physicsasset;
    PartRestoreSetup setup;
    setup.hasActor = setup.dynamicCompatible = setup.jointAttached = true;
    setup.ownerAnimated = setup.actorKinematic = setup.collisionRequested = setup.uniformScale = true;
    setup.driveStrength = 150.0f;
    setup.driveDamping = 6.0f;
    setup.driveMaxForce = 4000.0f;
    assert(CanRestoreInteractivePart(setup, 0, 0));
    assert(CanRestoreInteractivePart(setup, 1, 0));
    assert(CanRestoreInteractivePart(setup, 2, 0));
    assert(CanRestoreInteractivePart(setup, 2, 1));
    assert(!CanRestoreInteractivePart(setup, 1, 1)); // invalid detached Driven snapshot
    assert(!CanRestoreInteractivePart(setup, 0, 1));
    assert(!CanRestoreInteractivePart(setup, 2, 2));
    assert(!CanRestoreInteractivePart(setup, 3, 0));
    assert(!CanRestoreInteractivePart(setup, -1, 0));

    auto bad = setup;
    bad.jointAttached = false;
    assert(!CanRestoreInteractivePart(bad, 1, 0));
    assert(!CanRestoreInteractivePart(bad, 2, 0));
    assert(!CanRestoreInteractivePart(bad, 2, 1));
    assert(CanRestoreInteractivePart(bad, 0, 0));
    bad = setup; bad.dynamicCompatible = false;
    assert(!CanRestoreInteractivePart(bad, 2, 0));
    bad = setup; bad.uniformScale = false;
    assert(!CanRestoreInteractivePart(bad, 2, 0));
    bad = setup; bad.collisionRequested = false;
    assert(!CanRestoreInteractivePart(bad, 2, 0));
    bad = setup; bad.articulatedOverride = true;
    assert(!CanRestoreInteractivePart(bad, 0, 0));
    assert(!CanRestoreInteractivePart(bad, 2, 0));
    bad = setup; bad.alreadyDetached = true;
    assert(!CanRestoreInteractivePart(bad, 2, 1));
    bad = setup; bad.actorKinematic = false;
    assert(!CanRestoreInteractivePart(bad, 2, 0));
    bad = setup; bad.ownerAnimated = false;
    assert(!CanRestoreInteractivePart(bad, 2, 0));
    bad = setup; bad.hasActor = false;
    assert(!CanRestoreInteractivePart(bad, 0, 0));
    bad = setup; bad.driveStrength = 0;
    assert(!CanRestoreInteractivePart(bad, 1, 0));
    assert(CanRestoreInteractivePart(bad, 2, 0));
    bad = setup; bad.driveStrength = std::numeric_limits<float>::quiet_NaN();
    assert(!CanRestoreInteractivePart(bad, 1, 0));
    bad = setup; bad.driveMaxForce = -1;
    assert(!CanRestoreInteractivePart(bad, 1, 0));

    const float qnan = std::numeric_limits<float>::quiet_NaN();
    float pose[7] = {12, 3, 4, 0, 0, 0, 1};
    float lin[3] = {1, 2, 3};
    float ang[3] = {4, 5, 6};
    assert(ValidRestoredPartSnapshot(2.0f, pose, lin, ang));
    assert(!ValidRestoredPartSnapshot(-1, pose, lin, ang));
    pose[6] = 0.2f;
    assert(!ValidRestoredPartSnapshot(2, pose, lin, ang));
    pose[6] = 1;
    pose[0] = qnan;
    assert(!ValidRestoredPartSnapshot(2, pose, lin, ang));
    pose[0] = 12;
    lin[0] = std::numeric_limits<float>::infinity();
    assert(!ValidRestoredPartSnapshot(2, pose, lin, ang));

    // The format accepts a spherical (zero cylinder length) capsule.
    AssetDef asset; asset.version = 5;
    BodyDef body; body.name = "collision_sphere_capsule"; body.bone = "root";
    body.shape.type = ShapeType::Capsule;
    body.shape.radius = 5.0f;
    body.shape.halfHeight = 0.0f;
    asset.bodies.push_back(body);
    std::string error;
    assert(Validate(asset, &error));
    AssetDef parsed;
    assert(Parse(Serialize(asset), parsed, &error));
    assert(parsed.bodies.size() == 1 && parsed.bodies[0].shape.halfHeight == 0.0f);
    parsed.bodies[0].shape.halfHeight = -0.01f;
    assert(!Validate(parsed, &error));
    parsed.bodies[0].shape.halfHeight = 0.0f;
    parsed.bodies[0].shape.radius = 0.0f;
    assert(!Validate(parsed, &error));
    std::cout << "I.10.9 interactive restore eligibility, snapshot safety and zero-height capsule PASS\n";
}
