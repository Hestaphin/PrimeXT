#include "physics_part_drive.h"
#include "physics_asset_format.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <string>
int main() {
    using namespace physicsasset;
    constexpr float pi = 3.14159265f;
    assert(std::fabs(WrapHingeAngle(2.0f*pi)) < 1e-5f);
    assert(std::fabs(HingeTargetRate(-pi+0.05f,pi-0.05f,0.1f)-1.0f)<0.01f);
    assert(HingeTargetRate(1,0,0) == 0);
    assert(HingeTargetRate(1,0,10) == 0);
    assert(HingeTargetRate(1,0,0.01f) == 20.0f);
    auto c=ComputeHingeDrive(1,0,0,0,5,1,400);
    assert(c.valid && std::fabs(c.velocity-5.0f)<1e-5f && c.forceLimit==400);
    c=ComputeHingeDrive(0,1,0,1,5,2,400);
    assert(c.valid && c.velocity < -6.9f && c.velocity > -7.1f);
    c=ComputeHingeDrive(3,0,0,0,10000,2,5000000);
    assert(c.valid && c.velocity==20.0f && c.forceLimit==1000000.0f);
    assert(!ComputeHingeDrive(0,0,0,0,0,1,400).valid);
    assert(!ComputeHingeDrive(0,0,0,0,5,-1,400).valid);
    assert(!ComputeHingeDrive(0,0,0,0,5,1,0).valid);
    assert(!ComputeHingeDrive(0,0,std::numeric_limits<float>::infinity(),0,5,1,400).valid);
    AssetDef asset;
    asset.version=4;
    BodyDef chassis; chassis.name="chassis"; chassis.bone="root"; asset.bodies.push_back(chassis);
    BodyDef door; door.name="door"; door.bone="door_l"; door.propDrive=PropDriveMode::Driven;
    door.driveStrength=120; door.driveDamping=15; door.driveMaxForce=850;
    asset.bodies.push_back(door);
    JointDef hinge; hinge.name="hinge"; hinge.parent="chassis"; hinge.child="door";
    hinge.kind=JointKind::Hinge; hinge.hingeMin=-5; hinge.hingeMax=75;
    asset.joints.push_back(hinge);
    std::string error;
    assert(Validate(asset,&error));
    const std::string saved=Serialize(asset);
    assert(saved.find("drive_max_force 850")!=std::string::npos);
    AssetDef loaded;
    assert(Parse(saved,loaded,&error));
    assert(loaded.bodies.size()==2 && loaded.bodies[1].driveMaxForce==850);
    loaded.bodies[1].driveMaxForce=-1;
    assert(!Validate(loaded,&error));
    loaded.bodies[1].driveMaxForce=std::numeric_limits<float>::quiet_NaN();
    assert(!Validate(loaded,&error));
    loaded.bodies[1].driveMaxForce=0;
    assert(Validate(loaded,&error)); // backwards compatible v4 default
}
