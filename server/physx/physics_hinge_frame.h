#pragma once

// Shared hinge-frame construction for interactive props and articulated bodies.
// The pivot and direction are expressed in the CHILD bone's reference space.
// PhysX revolute joints rotate about their local X axis.
#include "physics_asset_format.h"
#include <PxPhysicsAPI.h>
#include <cmath>

namespace physicsasset
{
inline physx::PxTransform ChildHingeFrame(const JointDef &joint, float uniformScale = 1.0f)
{
    using namespace physx;
    const PxVec3 axis(joint.hingeAxis.x, joint.hingeAxis.y, joint.hingeAxis.z);
    const PxVec3 x = axis.getNormalized(); // schema validates nonzero finite axis
    const PxVec3 seed = std::fabs(x.z) < 0.95f ? PxVec3(0, 0, 1) : PxVec3(0, 1, 0);
    const PxVec3 y = (seed - x * x.dot(seed)).getNormalized();
    const PxVec3 z = x.cross(y);
    return PxTransform(PxVec3(joint.hingeAnchor.x * uniformScale, joint.hingeAnchor.y * uniformScale, joint.hingeAnchor.z * uniformScale),
        PxQuat(PxMat33(x, y, z)));
}

inline physx::PxRevoluteJoint *CreateRevoluteJoint(physx::PxPhysics &physics,
    physx::PxRigidActor &parent, physx::PxRigidDynamic &child,
    const physx::PxTransform &parentFrame, const physx::PxTransform &childFrame,
    const JointDef &definition)
{
    using namespace physx;
    PxRevoluteJoint *joint = PxRevoluteJointCreate(physics, &parent, parentFrame, &child, childFrame);
    if(!joint) return nullptr;
    joint->setLimit(PxJointAngularLimitPair(PxPi * definition.hingeMin / 180.0f,
        PxPi * definition.hingeMax / 180.0f));
    joint->setRevoluteJointFlag(PxRevoluteJointFlag::eLIMIT_ENABLED, true);
    // Parent/child contact is also filtered by their common asset owner.
    joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, false);
    // Break impulse remains metadata until I.7's damage/breakage policy.
    return joint;
}
}
