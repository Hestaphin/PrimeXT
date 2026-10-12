#pragma once
#include "physics_asset_format.h"
#include "ragdoll_geometry.h"
#include <PxPhysicsAPI.h>
namespace physicsasset
{
inline physx::PxTransform ChildD6Frame(const JointDef &joint,float uniformScale=1)
{
    using namespace physx;
    const auto basis=raggeometry::Euler(joint.frameAngles);
    return PxTransform(PxVec3(joint.frameAnchor.x*uniformScale,joint.frameAnchor.y*uniformScale,joint.frameAnchor.z*uniformScale),
        PxQuat(PxMat33(PxVec3(basis.x.x,basis.x.y,basis.x.z),PxVec3(basis.y.x,basis.y.y,basis.y.z),PxVec3(basis.z.x,basis.z.y,basis.z.z))));
}
}
