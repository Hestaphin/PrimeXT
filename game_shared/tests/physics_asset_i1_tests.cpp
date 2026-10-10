// Standalone Milestone I.1 regression suite:
// Compile with g++ -std=c++17 -Wall -Wextra -pedantic -Igame_shared,
// linking game_shared/physics_asset_format.cpp and this source into an executable.
// Run with assertions enabled (without -DNDEBUG).
#include "physics_asset_format.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>

using namespace physicsasset;

static BodyDef Body(const char *name, const char *bone)
{
    BodyDef b;
    b.name = name;
    b.bone = bone;
    b.shape.type = ShapeType::Box;
    return b;
}

static AssetDef ArticulatedAsset()
{
    AssetDef asset;
    asset.version = 4;
    asset.bodies.push_back(Body("chassis", "car_root"));
    BodyDef door = Body("left_door", "door_l");
    door.part = "left_door";
    door.propDrive = PropDriveMode::Driven;
    door.driveStrength = 120.0f;
    door.driveDamping = 15.0f;
    door.hitGroup = 20;
    asset.bodies.push_back(door);
    JointDef hinge;
    hinge.name = "left_hinge";
    hinge.parent = "chassis";
    hinge.child = "left_door";
    hinge.kind = JointKind::Hinge;
    hinge.hingeMin = -5.0f;
    hinge.hingeMax = 75.0f;
    hinge.breakImpulse = 900.0f;
    asset.joints.push_back(hinge);
    return asset;
}

static void InvalidInput(const char *text)
{
    AssetDef parsed;
    std::string err;
    assert(!Parse(text, parsed, &err));
    assert(!err.empty());
}

static void TestRoundTrip()
{
    AssetDef asset = ArticulatedAsset();
    std::string err;
    assert(Validate(asset, &err));
    AssetDef parsed;
    assert(Parse(Serialize(asset), parsed, &err));
    assert(parsed.version == 4 && parsed.bodies.size() == 2 && parsed.joints.size() == 1);
    assert(parsed.bodies[1].propDrive == PropDriveMode::Driven);
    assert(parsed.bodies[1].part == "left_door");
    assert(parsed.joints[0].kind == JointKind::Hinge);

    // Previously six-digit serialization silently rounded legitimate authored
    // values. All representable float values must survive the roundtrip.
    asset.bodies[1].mass = 1.23456788f;
    asset.bodies[1].driveStrength = 0.000123456789f;
    asset.bodies[1].offset.x = -1234.56787109375f;
    asset.joints[0].breakImpulse = 123456.789f;
    assert(Parse(Serialize(asset), parsed, &err));
    assert(parsed.bodies[1].mass == asset.bodies[1].mass);
    assert(parsed.bodies[1].driveStrength == asset.bodies[1].driveStrength);
    assert(parsed.bodies[1].offset.x == asset.bodies[1].offset.x);
    assert(parsed.joints[0].breakImpulse == asset.joints[0].breakImpulse);
}

static void TestParserRejectsEmptyNumbers()
{
    // strtof("", &end) and strtol("", &end) used to yield zero with
    // end == input, and the old parser did not check that case.
    InvalidInput("physics_asset { version \"\" body x { bone y } }");
    InvalidInput("physics_asset { version 4 body x { bone y mass \"\" } }");
    InvalidInput("physics_asset { version 4 body x { bone y hitgroup \"\" } }");
    InvalidInput("physics_asset { version 4 collision x { type triangle_mesh vertex \"\" 0 0 } }");
}

static void TestProgrammaticValidation()
{
    std::string err;
    AssetDef a = ArticulatedAsset();
    a.bodies[1].propDrive = static_cast<PropDriveMode>(-1);
    assert(!Validate(a, &err));
    a = ArticulatedAsset();
    a.joints[0].kind = static_cast<JointKind>(777);
    assert(!Validate(a, &err));
    a = ArticulatedAsset();
    a.bodies[0].shape.type = static_cast<ShapeType>(777);
    assert(!Validate(a, &err));
    a = ArticulatedAsset();
    a.bodies[0].part = "LEFT_DOOR"; // part identifiers are case-insensitive
    assert(!Validate(a, &err));
    a = ArticulatedAsset();
    CollisionDef collision;
    collision.name = "foo";
    collision.type = static_cast<CollisionGeometryType>(-1);
    a.collisions.push_back(collision);
    assert(!Validate(a, &err));
    a = ArticulatedAsset();
    a.joints[0].hingeMax = a.joints[0].hingeMin;
    assert(!Validate(a, &err));
}

static void TestBackwardCompatibility()
{
    std::string err;
    // All three earlier on-disk versions remain loadable.
    for(int version : {1, 2, 3})
    {
        AssetDef a;
        a.version = version;
        a.bodies.push_back(Body("pelvis", "Bip01 Pelvis"));
        AssetDef parsed;
        assert(Validate(a, &err));
        assert(Parse(Serialize(a), parsed, &err));
        assert(parsed.version == version);
        assert(parsed.bodies.size() == 1);
    }
    AssetDef collisionOnly;
    collisionOnly.version = 3;
    CollisionDef disabled;
    disabled.name = "door_removed";
    disabled.disabled = true;
    disabled.bone = "door_l";
    disabled.bodyGroup = "doors";
    disabled.variant = 0;
    collisionOnly.collisions.push_back(disabled);
    AssetDef parsed;
    assert(Validate(collisionOnly, &err));
    assert(Parse(Serialize(collisionOnly), parsed, &err));
    assert(parsed.collisions[0].disabled && parsed.collisions[0].bone == "door_l");
}

int main()
{
    TestRoundTrip();
    TestParserRejectsEmptyNumbers();
    TestProgrammaticValidation();
    TestBackwardCompatibility();
    std::cout << "PASS I.1: v1-v4 compatibility, v4 roundtrip, float precision, invalid inputs and enums\n";
}
