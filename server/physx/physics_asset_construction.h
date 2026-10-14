#pragma once
// Shared construction/configuration. Callers retain pose, scene and gameplay ownership.
#include "physics_asset_settings.h"
#include "physics_asset_contact_policy.h"
#include "physics_hinge_frame.h"
#include "physics_d6_frame.h"
#include "physics_asset_motion.h"
#include <PxPhysicsAPI.h>
namespace physicsasset
{
inline physx::PxTolerancesScale AssetTolerancesScale() {physx::PxTolerancesScale s;s.length=kPhysicsLength;s.speed=kPhysicsSpeed;return s;}
inline physx::PxMaterial *CreateAssetMaterial(physx::PxPhysics &physics)
{return physics.createMaterial(kAssetStaticFriction,kAssetDynamicFriction,kAssetRestitution);}
inline bool ConfigureBodyContact(physx::PxShape &shape,const AssetDef &asset,const BodyDef &body,float scale=1)
{
    const auto s=ResolveBodySettings(asset,body);
    if(!ValidBodyTuning(body.tuning)||!ValidSimulationSettings(s)||!std::isfinite(scale)||scale<=0||!std::isfinite(s.contactOffset*scale)||!std::isfinite(s.restOffset*scale)) return false;
    shape.setRestOffset(0);shape.setContactOffset(s.contactOffset*scale);shape.setRestOffset(s.restOffset*scale);return true;
}
inline bool ConfigureBodyStability(physx::PxRigidDynamic &actor,const AssetDef &asset,const BodyDef &body,float scale=1)
{
    const auto s=ResolveBodySettings(asset,body);const double square=double(scale)*scale;
    if(!ValidBodyTuning(body.tuning)||!ValidSimulationSettings(s)||!std::isfinite(scale)||scale<=0||!std::isfinite(float(s.maxLinearVelocity*scale))||!std::isfinite(float(s.maxDepenetrationVelocity*scale))||!std::isfinite(float(s.sleepThreshold*square))) return false;
    actor.setLinearDamping(s.linearDamping);actor.setAngularDamping(s.angularDamping);
    actor.setSolverIterationCounts(physx::PxU32(s.positionIterations),physx::PxU32(s.velocityIterations));
    actor.setMaxLinearVelocity(s.maxLinearVelocity*scale);actor.setMaxAngularVelocity(s.maxAngularVelocity);
    actor.setMaxDepenetrationVelocity(s.maxDepenetrationVelocity*scale);actor.setSleepThreshold(float(s.sleepThreshold*square));
    // The SDK forbids CCD on a kinematic actor. Handoff restores the profile.
    actor.setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_CCD,s.ccd&&!actor.getRigidBodyFlags().isSet(physx::PxRigidBodyFlag::eKINEMATIC));return true;
}
inline bool CreatePrimitiveBody(physx::PxPhysics &physics,physx::PxMaterial &material,const physx::PxTransform &pose,const AssetDef &asset,size_t index,float scale,physx::PxRigidDynamic *&outActor,physx::PxShape *&outShape)
{
    using namespace physx;
    if(index>=asset.bodies.size()||!std::isfinite(scale)||scale<=0||!pose.isValid()) return false;
    const auto &body=asset.bodies[index];if(!std::isfinite(body.mass)||body.mass<=0) return false;
    auto *actor=physics.createRigidDynamic(pose);if(!actor) return false;PxShape *shape=nullptr;
    switch(body.shape.type)
    {
    case ShapeType::Box:{const PxBoxGeometry g(PxVec3(body.shape.halfExtents.x*scale,body.shape.halfExtents.y*scale,body.shape.halfExtents.z*scale));if(g.isValid()) shape=PxRigidActorExt::createExclusiveShape(*actor,g,material);break;}
    case ShapeType::Sphere:{const PxSphereGeometry g(body.shape.radius*scale);if(g.isValid()) shape=PxRigidActorExt::createExclusiveShape(*actor,g,material);break;}
    case ShapeType::Capsule:{const PxCapsuleGeometry g(body.shape.radius*scale,body.shape.halfHeight*scale);if(g.isValid()) shape=PxRigidActorExt::createExclusiveShape(*actor,g,material);break;}
    }
    if(!shape||!ConfigureBodyContact(*shape,asset,body,scale)||!ConfigureBodyStability(*actor,asset,body,scale)||!PxRigidBodyExt::setMassAndUpdateInertia(*actor,body.mass)) {actor->release();return false;}
    actor->setName(body.name.c_str());shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE,false);outActor=actor;outShape=shape;return true;
}
inline bool ValidResolvedJointSettings(const AssetDef &asset,const JointDef &def,float scale)
{
    if(!std::isfinite(scale)||scale<=0||!ValidResolvedJointSettings(asset,def)) return false;
    const auto t=ResolveJointSettings(asset,def);const double square=double(scale)*scale;
    return std::isfinite(float(t.angularResistance*square))&&std::isfinite(float(t.maxTorque*square))&&std::isfinite(float(t.projectionLinearTolerance*scale));
}
inline bool ConfigureD6Joint(physx::PxD6Joint &joint,const AssetDef &asset,const JointDef &def,float scale,bool contacts)
{
    using namespace physx;
    if(!ValidResolvedJointSettings(asset,def,scale)||def.kind!=JointKind::D6||!AngularMotionName(def.twistMotion)||!AngularMotionName(def.swing1Motion)||!AngularMotionName(def.swing2Motion)||
        !std::isfinite(def.twistLow)||!std::isfinite(def.twistHigh)||def.twistLow< -180||def.twistHigh>180||def.twistLow>=def.twistHigh||!std::isfinite(def.swing1)||!std::isfinite(def.swing2)||def.swing1<=0||def.swing1>=180||def.swing2<=0||def.swing2>=180) return false;
    const auto t=ResolveJointSettings(asset,def);const double square=double(scale)*scale;
    joint.setMotion(PxD6Axis::eX,PxD6Motion::eLOCKED);joint.setMotion(PxD6Axis::eY,PxD6Motion::eLOCKED);joint.setMotion(PxD6Axis::eZ,PxD6Motion::eLOCKED);ConfigureD6AngularMotion(joint,def);
    joint.setTwistLimit(PxJointAngularLimitPair(PxPi*def.twistLow/180,PxPi*def.twistHigh/180));joint.setSwingLimit(PxJointLimitCone(PxPi*def.swing1/180,PxPi*def.swing2/180));
    // Split velocity-only drives remain meaningful with locked secondary axes.
    const PxD6JointDrive resistance(0,float(t.angularResistance*square),float(t.maxTorque*square),false);
    joint.setDrive(PxD6Drive::eTWIST,resistance);joint.setDrive(PxD6Drive::eSWING,resistance);joint.setDriveVelocity(PxVec3(0),PxVec3(0),false);
    joint.setConstraintFlag(PxConstraintFlag::eDRIVE_LIMITS_ARE_FORCES,true);joint.setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED,contacts);
    joint.setProjectionLinearTolerance(t.projectionLinearTolerance*scale);joint.setProjectionAngularTolerance(PxPi*t.projectionAngularTolerance/180);joint.setConstraintFlag(PxConstraintFlag::ePROJECTION,t.projection==ProjectionMode::On);return true;
}
inline physx::PxJoint *CreateAssetJoint(physx::PxPhysics &physics,physx::PxRigidActor &parent,physx::PxRigidDynamic &child,const physx::PxTransform &parentReferenceBody,const physx::PxTransform &childReferenceBody,const physx::PxTransform &childReferenceBone,const AssetDef &asset,const JointDef &def,float scale=1)
{
    using namespace physx;
    const int a=FindBody(asset,def.parent.c_str()),b=FindBody(asset,def.child.c_str());
    if(a<0||b<0||a==b||!ValidResolvedJointSettings(asset,def,scale)||!parentReferenceBody.isValid()||!childReferenceBody.isValid()||!childReferenceBone.isValid()||(def.kind!=JointKind::D6&&def.kind!=JointKind::Hinge)) return nullptr;
    if(def.kind==JointKind::Hinge)
    {
        const double length=double(def.hingeAxis.x)*def.hingeAxis.x+double(def.hingeAxis.y)*def.hingeAxis.y+double(def.hingeAxis.z)*def.hingeAxis.z;
        if(!std::isfinite(length)||length<=1e-12||length>=1e12||!std::isfinite(def.hingeMin)||!std::isfinite(def.hingeMax)||def.hingeMin< -180||def.hingeMax>180||def.hingeMin>=def.hingeMax||def.tuning.angularResistance>0) return nullptr;
    }
    const bool contacts=CollisionPairEnabled(asset,a,b);
    const PxTransform frame=childReferenceBone*(def.kind==JointKind::Hinge?ChildHingeFrame(def,scale):ChildD6Frame(def,scale));
    const PxTransform f0=parentReferenceBody.getInverse()*frame,f1=childReferenceBody.getInverse()*frame;if(!f0.isValid()||!f1.isValid()) return nullptr;
    if(def.kind==JointKind::Hinge)
    {
        auto *joint=CreateRevoluteJoint(physics,parent,child,f0,f1,def);if(!joint) return nullptr;const auto t=ResolveJointSettings(asset,def);
        joint->setProjectionLinearTolerance(t.projectionLinearTolerance*scale);joint->setProjectionAngularTolerance(PxPi*t.projectionAngularTolerance/180);
        joint->setConstraintFlag(PxConstraintFlag::ePROJECTION,t.projection==ProjectionMode::On);joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED,contacts);return joint;
    }
    auto *joint=PxD6JointCreate(physics,&parent,f0,&child,f1);if(joint&&!ConfigureD6Joint(*joint,asset,def,scale,contacts)) {joint->release();joint=nullptr;}return joint;
}
}
