// Standalone Milestone I.9 editor integration/authoring regression test.
// Tests the shared schema used by pxmv to save v4 interactive assets. This
// does NOT simulate PhysX or exercise a GUI toolkit.
#include "physics_asset_format.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

int main(int argc, char **argv)
{
    using namespace physicsasset;
    assert(argc == 2);
    AssetDef asset;
    std::string err;
    assert(LoadFile(argv[1], asset, &err));
    assert(asset.version == 4);
    assert(asset.bodies.size() == 2 && asset.joints.size() == 1);
    assert(asset.bodies[0].name == "chassis");
    const BodyDef &door = asset.bodies[1];
    assert(door.name == "left_door" && door.bone == "door_l");
    assert(door.part == "left_door" && door.propDrive == PropDriveMode::Driven);
    assert(door.driveStrength == 120 && door.driveDamping == 15 && door.driveMaxForce == 850);
    const JointDef &hinge = asset.joints[0];
    assert(hinge.kind == JointKind::Hinge && hinge.child == door.name);
    assert(hinge.hingeAxis.z == 1 && hinge.hingeMin == -5 && hinge.hingeMax == 75);
    assert(hinge.releaseDamage == 30 && hinge.breakDamage == 100 && hinge.breakImpulse == 900);
    AssetDef parsed;
    assert(Parse(Serialize(asset), parsed, &err));
    assert(parsed.bodies[1].part == "left_door");
    assert(parsed.joints[0].hingeAxis.z == 1);
    parsed.joints[0].hingeAxis = {};
    assert(!Validate(parsed, &err));
    parsed = asset;
    parsed.joints[0].breakDamage = -1;
    assert(!Validate(parsed, &err));
    parsed = asset;
    parsed.bodies[1].driveStrength = 0;
    assert(!Validate(parsed, &err));
    parsed = asset;
    parsed.bodies[1].part = "chassis";
    parsed.bodies[0].part = "chassis";
    assert(!Validate(parsed, &err));
    std::cout << "I.9 example v4 validation/round-trip PASS\n";
}
