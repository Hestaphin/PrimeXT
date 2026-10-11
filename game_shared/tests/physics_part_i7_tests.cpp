#include "physics_part_failure.h"
#include "physics_asset_format.h"
#include <cassert>
#include <limits>
#include <string>
int main() {
    using namespace physicsasset;
    PartFailurePolicy policy{30.0f, 100.0f, 200.0f};
    auto r = EvaluatePartFailure(policy, 0, 10, 50, false);
    assert(r.valid && r.accumulatedDamage == 10 && !r.release && !r.detach);
    r = EvaluatePartFailure(policy, 10, 20, 50, false);
    assert(r.valid && r.accumulatedDamage == 30 && r.release && !r.detach);
    r = EvaluatePartFailure(policy, 90, 10, 1, false);
    assert(r.valid && r.detach && r.release);
    r = EvaluatePartFailure(policy, 0, 1, 200, false);
    assert(r.valid && r.detach && r.accumulatedDamage == 1);
    r = EvaluatePartFailure(policy, 100, 100, 999, true);
    assert(r.valid && !r.detach && !r.release);
    r = EvaluatePartFailure({}, 0, 500, 10000, false);
    assert(r.valid && r.accumulatedDamage==500 && !r.detach && !r.release);
    r = EvaluatePartFailure(policy, 0, std::numeric_limits<float>::infinity(), 1, false);
    assert(!r.valid);
    r = EvaluatePartFailure(policy, -1, 2, 3, false);
    assert(!r.valid);
    r = EvaluatePartFailure(policy, 999999999.f, 999999999.f, 5, false);
    assert(r.valid && r.accumulatedDamage == 1000000000.0f);

    AssetDef asset; asset.version=4;
    BodyDef base; base.name="chassis"; base.bone="root"; asset.bodies.push_back(base);
    BodyDef door; door.name="door"; door.bone="door_l"; door.part="left_door";
    door.propDrive=PropDriveMode::Driven; door.driveStrength=120; asset.bodies.push_back(door);
    JointDef joint; joint.name="door_hinge"; joint.parent="chassis"; joint.child="door";
    joint.kind=JointKind::Hinge; joint.hingeMin=-5; joint.hingeMax=75;
    joint.releaseDamage=30; joint.breakDamage=100; joint.breakImpulse=200;
    asset.joints.push_back(joint);
    std::string error;
    assert(Validate(asset, &error));
    const std::string saved = Serialize(asset);
    assert(saved.find("release_damage 30") != std::string::npos);
    assert(saved.find("break_damage 100") != std::string::npos);
    AssetDef loaded;
    assert(Parse(saved, loaded, &error));
    assert(loaded.joints[0].releaseDamage==30 && loaded.joints[0].breakDamage==100);
    loaded.joints[0].breakDamage=-2;
    assert(!Validate(loaded, &error));
    loaded.joints[0].breakDamage=std::numeric_limits<float>::infinity();
    assert(!Validate(loaded, &error));
    loaded.joints[0].breakDamage=100;
    loaded.joints[0].kind=JointKind::D6;
    assert(!Validate(loaded, &error));
    loaded.joints[0].kind=JointKind::Hinge;
    loaded.version=3;
    assert(!Validate(loaded, &error));
    loaded.version=4;
    loaded.joints[0].releaseDamage=0; loaded.joints[0].breakDamage=0; loaded.joints[0].breakImpulse=0;
    assert(Validate(loaded, &error)); // backwards-compatible no-threshold v4
}
