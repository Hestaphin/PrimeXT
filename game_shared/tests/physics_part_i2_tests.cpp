// Milestone I.2 pure layout regression suite (no PhysX SDK needed).
#include "physics_part_layout.h"
#include <cassert>
#include <iostream>

using namespace physicsasset;

static CollisionDef Section(const char *name, const char *bone, int variant = -1)
{
    CollisionDef c;
    c.name = name;
    c.bone = bone;
    if(variant >= 0) { c.bodyGroup = "door_style"; c.variant = variant; }
    c.type = CollisionGeometryType::Convex;
    c.vertices = {{0,0,0},{1,0,0},{0,1,0}};
    c.indices = {0,1,2};
    return c;
}

static BodyDef Body(const char *bone, const char *part)
{
    BodyDef b;
    b.name = part;
    b.bone = bone;
    b.part = part;
    b.propDrive = PropDriveMode::Driven;
    b.driveStrength = 120.0f;
    b.shape.type = ShapeType::Box;
    return b;
}

int main()
{
    AssetDef a;
    a.collisions.push_back(Section("chassis", ""));
    a.collisions.push_back(Section("door_outer", "door_l", 0));
    a.collisions.push_back(Section("door_inner", "DOOR_L", 0));
    a.collisions.push_back(Section("door_variant", "Door_L", 1));
    a.collisions.push_back(Section("hood", "hood_bone"));
    a.bodies.push_back(Body("door_l", "left_door"));
    // An unrelated ragdoll body must not create an extra interactive part.
    BodyDef legacy;
    legacy.name = "pelvis";
    legacy.bone = "pelvis";
    a.bodies.push_back(legacy);
    auto parts = BuildPartLayout(a);
    assert(parts.size() == 2);
    assert(parts[0].name == "left_door");
    assert(parts[0].bodyIndex == 0);
    assert(parts[0].initialMode == PropDriveMode::Driven);
    assert(parts[0].collisionIndices.size() == 3);
    assert(parts[0].collisionIndices[0] == 1);
    assert(parts[0].collisionIndices[1] == 2);
    assert(parts[0].collisionIndices[2] == 3);
    assert(parts[1].name == "hood_bone");
    assert(parts[1].bodyIndex == -1);
    assert(parts[1].initialMode == PropDriveMode::Animated);

    // Interactive v4 metadata without imported collision geometry is still
    // registered but must NOT create a duplicate proxy or fabricated shape.
    a.collisions.clear();
    parts = BuildPartLayout(a);
    assert(parts.size() == 1);
    assert(parts[0].name == "left_door");
    assert(parts[0].collisionIndices.empty());

    a.collisions = { Section("chassis", "") };
    parts = BuildPartLayout(a);
    assert(parts.size() == 1); // root mesh does not create a bone part
    assert(parts[0].collisionIndices.empty());

    // Legacy v3 collision-only assets still get one actor per moving bone.
    a.bodies.clear();
    a.collisions = { Section("door_a", "door_l"), Section("door_b", "door_l") };
    parts = BuildPartLayout(a);
    assert(parts.size() == 1);
    assert(parts[0].collisionIndices.size() == 2);
    assert(parts[0].bodyIndex == -1);

    assert(SamePartBone("DoOr_L", "door_l"));
    assert(!SamePartBone("door_l", "door_r"));
    std::cout << "Milestone I.2 physics part layout: PASS\n";
}
