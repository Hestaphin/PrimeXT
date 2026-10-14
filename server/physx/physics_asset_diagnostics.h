#pragma once

// Read-only PhysX adapter shared by preview and runtime. Call AFTER fetchResults.
#include <PxPhysicsAPI.h>
#include "ragdoll_diagnostics.h"

namespace ragdolldiag
{
inline Vec3 DiagnosticVector(const physx::PxVec3 &v) { return {v.x,v.y,v.z}; }
inline Pose DiagnosticPose(const physx::PxTransform &p)
{
    return {DiagnosticVector(p.p),{p.q.x,p.q.y,p.q.z,p.q.w}};
}

inline void MeasureBody(const physx::PxRigidDynamic *body, Snapshot &out, bool root = false)
{
    if(!body) { ++out.missingBodies; return; }
    ++out.bodies;
    const bool kinematic = !!(body->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC);
    if(kinematic) ++out.kinematic;
    else if(body->isSleeping()) ++out.sleeping;
    else ++out.active;
    if(!body->getGlobalPose().isValid()) ++out.invalidSamples;
    const double linear = Length(DiagnosticVector(body->getLinearVelocity()));
    const double angular = Length(DiagnosticVector(body->getAngularVelocity()));
    if(linear < 0 || angular < 0 || !std::isfinite(linear) || !std::isfinite(angular)) ++out.invalidSamples;
    else { out.maxLinearSpeed = std::max(out.maxLinearSpeed,linear); out.maxAngularSpeed = std::max(out.maxAngularSpeed,angular); }
    if(root) { out.rootLinearSpeed = linear; out.rootAngularSpeed = angular; }
}

inline void MeasureJoint(const physx::PxJoint *joint, Snapshot &out, int index)
{
    using namespace physx;
    if(!joint) { ++out.missingJoints; return; }
    ++out.joints;
    if(joint->getConstraintFlags() & PxConstraintFlag::eBROKEN) { ++out.brokenJoints; return; }
    PxRigidActor *parent = nullptr, *child = nullptr;
    joint->getActors(parent,child);
    // A null actor denotes a world anchor, not an invalid entity pointer.
    const PxTransform a = (parent ? parent->getGlobalPose() : PxTransform(PxIdentity)) * joint->getLocalPose(PxJointActorIndex::eACTOR0);
    const PxTransform b = (child ? child->getGlobalPose() : PxTransform(PxIdentity)) * joint->getLocalPose(PxJointActorIndex::eACTOR1);
    if(!a.isValid() || !b.isValid()) { ++out.invalidSamples; return; }
    const double separation = Distance(DiagnosticVector(a.p),DiagnosticVector(b.p));
    if(separation < 0 || !std::isfinite(separation)) ++out.invalidSamples;
    else if(separation > out.maxAnchorSeparation)
    { out.maxAnchorSeparation = separation; out.worstAnchorJoint = index; }
    const double speed = Length(DiagnosticVector(joint->getRelativeAngularVelocity()));
    if(speed < 0 || !std::isfinite(speed)) ++out.invalidSamples;
    else out.maxRelativeAngularSpeed = std::max(out.maxRelativeAngularSpeed,speed);

    double excess = 0;
    if(const PxD6Joint *d6 = joint->is<PxD6Joint>())
    {
        const PxJointAngularLimitPair twist = d6->getTwistLimit();
        const PxJointLimitCone swing = d6->getSwingLimit();
        auto axis = [&](PxD6Axis::Enum dof, double angle, double low, double high)
        {
            const PxD6Motion::Enum motion = d6->getMotion(dof);
            if(motion == PxD6Motion::eFREE) return;
            const double value = IntervalExcess(angle,motion == PxD6Motion::eLOCKED ? 0 : low,
                motion == PxD6Motion::eLOCKED ? 0 : high);
            if(value < 0) ++out.invalidSamples;
            else excess = std::max(excess,value);
        };
        axis(PxD6Axis::eTWIST,d6->getTwistAngle(),twist.lower,twist.upper);
        axis(PxD6Axis::eSWING1,d6->getSwingYAngle(),-swing.yAngle,swing.yAngle);
        axis(PxD6Axis::eSWING2,d6->getSwingZAngle(),-swing.zAngle,swing.zAngle);
        // Each axis test is a LOWER BOUND. Two individually admissible swing
        // angles may still violate the elliptical cone. Do not label it PASS.
    }
    else if(const PxRevoluteJoint *hinge = joint->is<PxRevoluteJoint>())
    {
        if(hinge->getRevoluteJointFlags() & PxRevoluteJointFlag::eLIMIT_ENABLED)
        {
            const PxJointAngularLimitPair limit = hinge->getLimit();
            const double value = IntervalExcess(hinge->getAngle(),limit.lower,limit.upper);
            if(value < 0) ++out.invalidSamples;
            else excess = value;
        }
    }
    excess *= 180.0 / 3.14159265358979323846;
    if(excess > out.maxAxisExcessDegrees)
    { out.maxAxisExcessDegrees = excess; out.worstAxisJoint = index; }
}

inline void MeasureScene(const physx::PxSimulationStatistics *stats, Snapshot &out)
{
    if(!stats) return;
    out.sceneDiscretePairs = static_cast<int>(stats->nbDiscreteContactPairsTotal);
    out.scenePairsWithContacts = static_cast<int>(stats->nbDiscreteContactPairsWithContacts);
}
}
