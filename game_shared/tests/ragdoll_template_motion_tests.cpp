#include "../../server/physx/physics_asset_motion.h"
#include <cassert>
#include <iostream>
int main()
{
    using namespace physicsasset;using namespace physx;
    PxD6Joint joint;JointDef def;
    ConfigureD6AngularMotion(joint,def);
    for(int i=0;i<3;++i) assert(joint.writes[i]==0&&joint.motion[i]==PxD6Motion::eLOCKED);
    for(int i=3;i<6;++i) assert(joint.writes[i]==1&&joint.motion[i]==PxD6Motion::eLIMITED);
    def.twistMotion=def.swing1Motion=def.swing2Motion=AngularMotion::Free;
    ConfigureD6AngularMotion(joint,def);
    for(int i=0;i<3;++i) assert(joint.writes[i]==0&&joint.motion[i]==PxD6Motion::eLOCKED);
    for(int i=3;i<6;++i) assert(joint.writes[i]==2&&joint.motion[i]==PxD6Motion::eFREE);
    def.twistMotion=AngularMotion::Locked;def.swing1Motion=AngularMotion::Limited;
    ConfigureD6AngularMotion(joint,def);
    assert(joint.motion[3]==PxD6Motion::eLOCKED&&joint.motion[4]==PxD6Motion::eLIMITED&&joint.motion[5]==PxD6Motion::eFREE);
    assert(ResolveAngularMotion(static_cast<AngularMotion>(99))==PxD6Motion::eLOCKED);
    std::cout<<"PASS F1 production angular setter: limited/free/mixed, zero translation writes (facade; NO SDK/solver)\n";
}
