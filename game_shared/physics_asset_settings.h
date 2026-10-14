#pragma once
#include "physics_asset_format.h"
#include <algorithm>
#include <cmath>
namespace physicsasset
{
// Reference/editor-preview cadence. In-game scheduling uses PhysicsStepScheduler.
constexpr double kSimulationStepSeconds=1.0/100.0;
constexpr float kAssetStaticFriction=0.5f,kAssetDynamicFriction=0.5f,kAssetRestitution=0.0f;
inline SimulationSettings ResolveBodySettings(const AssetDef &asset,const BodyDef &body)
{
    auto s=asset.simulation;
    if(body.linearDamping>=0) s.linearDamping=body.linearDamping;
    if(body.angularDamping>=0) s.angularDamping=body.angularDamping;
    const auto &o=body.tuning;
    if(o.positionIterations>=0) s.positionIterations=o.positionIterations;
    if(o.velocityIterations>=0) s.velocityIterations=o.velocityIterations;
    if(o.maxLinearVelocity>=0) s.maxLinearVelocity=o.maxLinearVelocity;
    if(o.maxAngularVelocity>=0) s.maxAngularVelocity=o.maxAngularVelocity;
    if(o.maxDepenetrationVelocity>=0) s.maxDepenetrationVelocity=o.maxDepenetrationVelocity;
    if(o.sleepThreshold>=0) s.sleepThreshold=o.sleepThreshold;
    if(o.contactOffset>=0) s.contactOffset=o.contactOffset;
    if(o.restOffset>=0) s.restOffset=o.restOffset;
    if(o.ccd>=0) s.ccd=o.ccd!=0;
    return s;
}
inline JointTuning ResolveJointSettings(const AssetDef &asset,const JointDef &joint)
{
    auto t=joint.tuning;
    if(t.angularResistance<0) t.angularResistance=asset.simulation.angularResistance;
    if(t.maxTorque<0) t.maxTorque=asset.simulation.maxTorque;
    if(t.projection==ProjectionMode::Inherit) t.projection=asset.simulation.projection;
    if(t.projectionLinearTolerance<0) t.projectionLinearTolerance=asset.simulation.projectionLinearTolerance;
    if(t.projectionAngularTolerance<0) t.projectionAngularTolerance=asset.simulation.projectionAngularTolerance;
    return t;
}
inline void MaterializeBodySettings(const AssetDef &asset,BodyDef &body)
{
    const auto s=ResolveBodySettings(asset,body);body.linearDamping=s.linearDamping;body.angularDamping=s.angularDamping;
    body.tuning={s.positionIterations,s.velocityIterations,s.maxLinearVelocity,s.maxAngularVelocity,s.maxDepenetrationVelocity,s.sleepThreshold,s.ccd?1:0,s.contactOffset,s.restOffset};
}
inline bool ValidSimulationSettings(const SimulationSettings &s)
{
    const auto range=[](float v,float a,float b){return std::isfinite(v)&&v>=a&&v<=b;};
    return range(s.linearDamping,0,100)&&range(s.angularDamping,0,100)&&s.positionIterations>=1&&s.positionIterations<=255&&s.velocityIterations>=0&&s.velocityIterations<=255&&
        range(s.maxLinearVelocity,.001f,100000)&&range(s.maxAngularVelocity,.001f,1000)&&range(s.maxDepenetrationVelocity,.001f,100000)&&range(s.sleepThreshold,0,1000000)&&
        range(s.contactOffset,.00001f,1000)&&range(s.restOffset,0,1000)&&s.restOffset<s.contactOffset&&
        range(s.angularResistance,0,1000000)&&range(s.maxTorque,0,1000000000)&&(s.angularResistance==0||s.maxTorque>0)&&
        (s.projection==ProjectionMode::Off||s.projection==ProjectionMode::On)&&range(s.projectionLinearTolerance,.00001f,1000)&&range(s.projectionAngularTolerance,.01f,179.999f);
}
inline bool ValidBodyTuning(const BodyTuning &s)
{
    const auto unset=[](float v){return v==-1||(std::isfinite(v)&&v>=0);};
    return (s.positionIterations==-1||(s.positionIterations>=1&&s.positionIterations<=255))&&s.velocityIterations>=-1&&s.velocityIterations<=255&&
        unset(s.maxLinearVelocity)&&unset(s.maxAngularVelocity)&&unset(s.maxDepenetrationVelocity)&&unset(s.sleepThreshold)&&s.ccd>=-1&&s.ccd<=1&&unset(s.contactOffset)&&unset(s.restOffset);
}
inline bool ValidJointTuning(const JointTuning &s)
{
    const auto unset=[](float v){return v==-1||(std::isfinite(v)&&v>=0);};
    return unset(s.angularResistance)&&unset(s.maxTorque)&&unset(s.projectionLinearTolerance)&&unset(s.projectionAngularTolerance)&&
        (s.projection==ProjectionMode::Inherit||s.projection==ProjectionMode::Off||s.projection==ProjectionMode::On);
}
inline bool ValidResolvedJointSettings(const AssetDef &asset,const JointDef &joint)
{
    if(!ValidJointTuning(joint.tuning)) return false;
    const auto t=ResolveJointSettings(asset,joint);auto s=asset.simulation;
    s.angularResistance=t.angularResistance;s.maxTorque=t.maxTorque;s.projection=t.projection;
    s.projectionLinearTolerance=t.projectionLinearTolerance;s.projectionAngularTolerance=t.projectionAngularTolerance;
    return ValidSimulationSettings(s);
}
}
