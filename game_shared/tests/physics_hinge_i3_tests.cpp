// Portable I.3 authoring/hinge-schema regression tests. PhysX execution is
// intentionally separate: this test checks the part topology and v5 file data.
#include "physics_asset_format.h"
#include "physics_part_layout.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace physicsasset;

static AssetDef Door()
{
    AssetDef a;
    a.version = 5;
    BodyDef chassis;
    chassis.name = "chassis";
    chassis.bone = "car_root";
    chassis.shape.type = ShapeType::Box;
    a.bodies.push_back(chassis);
    BodyDef door;
    door.name = "left_door";
    door.part = "left_door";
    door.bone = "door_l";
    door.propDrive = PropDriveMode::Driven;
    door.driveStrength = 120.0f;
    door.mass = 18.0f;
    door.shape.type = ShapeType::Box;
    a.bodies.push_back(door);
    CollisionDef front;
    front.name = "outer";
    front.bone = "door_l";
    front.type = CollisionGeometryType::Convex;
    front.vertices = {{0,0,0},{1,0,0},{0,1,0},{0,0,1}};
    front.indices = {0,1,2,0,1,3,0,2,3,1,2,3};
    a.collisions.push_back(front);
    CollisionDef back = front;
    back.name = "inner";
    a.collisions.push_back(back);
    JointDef hinge;
    hinge.name = "driver_hinge";
    hinge.kind = JointKind::Hinge;
    hinge.parent = "chassis";
    hinge.child = "left_door";
    hinge.hingeMin = -5;
    hinge.hingeMax = 75;
    hinge.hingeAnchor = { 0.123456791f, 7.25f, -0.125f };
    hinge.hingeAxis = { 0, 0, 1 };
    a.joints.push_back(hinge);
    return a;
}

int main()
{
    std::string error;
    AssetDef original = Door();
    assert(Validate(original, &error));
    AssetDef output;
    assert(Parse(Serialize(original), output, &error));
    assert(output.joints.size() == 1);
    assert(output.joints[0].hingeAnchor.x == original.joints[0].hingeAnchor.x);
    assert(output.joints[0].hingeAnchor.y == 7.25f);
    assert(output.joints[0].hingeAxis.z == 1.0f);
    assert(output.joints[0].hingeMin == -5 && output.joints[0].hingeMax == 75);
    // New v5 assets omit the optional pivot/direction and use defaults.
    std::string legacyText = Serialize(original);
    // Strip complete optional lines without depending on float spellings.
    const auto stripLine = [](std::string &text, const std::string &field) {
        const auto start = text.find(field);
        assert(start != std::string::npos);
        const auto end = text.find('\n', start);
        assert(end != std::string::npos);
        text.erase(start, end - start + 1);
    };
    stripLine(legacyText, "        hinge_anchor ");
    stripLine(legacyText, "        hinge_axis ");
    AssetDef legacyH;
    assert(Parse(legacyText, legacyH, &error));
    assert(legacyH.joints[0].hingeAnchor.x == 0.0f);
    assert(legacyH.joints[0].hingeAxis.x == 1.0f);
    assert(legacyH.joints[0].hingeAxis.y == 0.0f);

    auto parts = BuildPartLayout(output);
    assert(parts.size() == 1); // root body uses entity chassis actor, not a second part
    int doorCount = 0;
    for(const auto &part : parts)
        if(part.name == "left_door") { ++doorCount; assert(part.collisionIndices.size() == 2); }
    assert(doorCount == 1); // two door shapes, one actor, one hinge

    AssetDef bad = Door();
    bad.joints[0].hingeAxis = {0,0,0};
    assert(!Validate(bad, &error));
    bad = Door();
    bad.joints[0].hingeAxis = {1e-10f,0,0};
    assert(!Validate(bad, &error));
    bad = Door();
    bad.joints[0].hingeAxis.x = std::nanf("");
    assert(!Validate(bad, &error));
    bad = Door();
    bad.joints[0].hingeMin = 91;
    assert(!Validate(bad, &error));
    bad = Door();
    bad.joints[0].child = "absent";
    assert(!Validate(bad, &error));

    AssetDef proxy;proxy.collisions.push_back(original.collisions[0]);assert(Validate(proxy,&error)&&Parse(Serialize(proxy),output,&error));assert(output.version==5);
    std::cout << "Milestone I.3 hinge schema/layout tests passed\n";
}
