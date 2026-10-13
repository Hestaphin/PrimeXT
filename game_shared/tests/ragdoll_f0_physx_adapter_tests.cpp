#include "../../server/physx/physics_asset_diagnostics.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace ragdolldiag;
using namespace physx;
static void Near(double a,double b){assert(std::fabs(a-b)<0.002);}
int main()
{
    PxRigidDynamic parent,child;
    parent.pose=PxTransform(PxVec3(0,0,0),PxQuat(0,0,0.70710678f,0.70710678f));
    child.pose=PxTransform(PxVec3(0,1,0));
    parent.linear=PxVec3(3,4,0); parent.angular=PxVec3(0,0,6);
    child.sleeping=true;
    Snapshot sample;
    MeasureBody(&parent,sample,true); MeasureBody(&child,sample); MeasureBody(nullptr,sample);
    assert(sample.bodies==2&&sample.active==1&&sample.sleeping==1&&sample.missingBodies==1);
    Near(sample.rootLinearSpeed,5); Near(sample.rootAngularSpeed,6);
    PxD6Joint joint;
    joint.parent=&parent; joint.child=&child;
    joint.a=PxTransform(PxVec3(1,0,0)); joint.b=PxTransform(PxIdentity);
    joint.relativeAngular=PxVec3(0,8,0);
    MeasureJoint(&joint,sample,0); Near(sample.maxAnchorSeparation,0); Near(sample.maxRelativeAngularSpeed,8);
    joint.twist=0.7f; child.pose.p.x=2;
    MeasureJoint(&joint,sample,1); Near(sample.maxAnchorSeparation,2);
    Near(sample.maxAxisExcessDegrees,0.2*180/3.14159265358979323846);
    assert(sample.worstAnchorJoint==1&&sample.worstAxisJoint==1);
    // Free angular motion has no limit violation, but anchors stay measured.
    for(auto &m:joint.motion)m=PxD6Motion::eFREE;
    Snapshot free; MeasureJoint(&joint,free,2); Near(free.maxAxisExcessDegrees,0); Near(free.maxAnchorSeparation,2);
    const unsigned parentReads=parent.reads;
    joint.flags=PxConstraintFlag::eBROKEN;
    Snapshot broken; MeasureJoint(&joint,broken,0); assert(broken.brokenJoints==1&&parent.reads==parentReads);
    MeasureJoint(nullptr,broken,1); assert(broken.missingJoints==1);
    PxRevoluteJoint hinge; hinge.child=&child; hinge.angle=-0.8f;
    Snapshot hs; MeasureJoint(&hinge,hs,0); Near(hs.maxAxisExcessDegrees,0.3*180/3.14159265358979323846);
    hinge.revoluteFlags=0; hs=Snapshot{}; MeasureJoint(&hinge,hs,0); Near(hs.maxAxisExcessDegrees,0);
    child.flags=PxRigidBodyFlag::eKINEMATIC; hs=Snapshot{}; MeasureBody(&child,hs); assert(hs.kinematic==1&&hs.sleeping==0);
    child.linear.x=std::numeric_limits<float>::quiet_NaN(); MeasureBody(&child,hs,true);
    assert(hs.invalidSamples==1&&hs.rootLinearSpeed==-1);
    PxSimulationStatistics stats; stats.nbDiscreteContactPairsTotal=17; stats.nbDiscreteContactPairsWithContacts=6;
    MeasureScene(&stats,hs); assert(hs.sceneDiscretePairs==17&&hs.scenePairsWithContacts==6);
    Snapshot absent; MeasureScene(nullptr,absent); assert(absent.sceneDiscretePairs==-1);
    std::cout << "PASS F0 production PhysX getter adapter: rotated local anchors, world anchor, angular modes, broken/missing joints, velocities and scene statistics (NO SDK/solver)\n";
}
