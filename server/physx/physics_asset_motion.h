#pragma once
#include <PxPhysicsAPI.h>
#include "physics_asset_format.h"
namespace physicsasset
{
inline physx::PxD6Motion::Enum ResolveAngularMotion(AngularMotion mode)
{
    switch(mode)
    {
    case AngularMotion::Locked: return physx::PxD6Motion::eLOCKED;
    case AngularMotion::Free: return physx::PxD6Motion::eFREE;
    case AngularMotion::Limited: return physx::PxD6Motion::eLIMITED;
    }
    return physx::PxD6Motion::eLOCKED; // invalid assets are rejected by Validate
}
inline void ConfigureD6AngularMotion(physx::PxD6Joint &joint, const JointDef &definition)
{
    joint.setMotion(physx::PxD6Axis::eTWIST,ResolveAngularMotion(definition.twistMotion));
    joint.setMotion(physx::PxD6Axis::eSWING1,ResolveAngularMotion(definition.swing1Motion));
    joint.setMotion(physx::PxD6Axis::eSWING2,ResolveAngularMotion(definition.swing2Motion));
}
}
