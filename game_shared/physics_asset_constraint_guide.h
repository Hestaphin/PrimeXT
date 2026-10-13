#pragma once
// Portable joint-frame/angle guides. Separate arcs show each angular axis;
// they are authoring guides, not a complete PhysX swing-cone error metric.
#include "ragdoll_geometry.h"
#include <utility>

namespace physicsasset
{
struct ConstraintGuideLine {Vec3 from,to,color;};
struct ConstraintGuide {raggeometry::Pose parent,child;std::vector<ConstraintGuideLine> lines;};
inline bool GuideFinite(Vec3 p) {return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline bool GuideRigid(const raggeometry::Pose &p)
{
    using namespace raggeometry;
    return GuideFinite(p.p)&&GuideFinite(p.x)&&GuideFinite(p.y)&&GuideFinite(p.z)&&
        std::fabs(Len(p.x)-1)<.002&&std::fabs(Len(p.y)-1)<.002&&std::fabs(Len(p.z)-1)<.002&&
        std::fabs(Dot(p.x,p.y))<.002&&std::fabs(Dot(p.x,p.z))<.002&&std::fabs(Dot(p.y,p.z))<.002;
}
inline raggeometry::Pose GuideBodyFrame(const raggeometry::Pose &reference,const raggeometry::Pose &current,const raggeometry::Pose &frame)
{
    return {current.Rotate(reference.InverseRotate(frame.x)),current.Rotate(reference.InverseRotate(frame.y)),
        current.Rotate(reference.InverseRotate(frame.z)),current.Point(reference.InversePoint(frame.p))};
}
inline bool BuildConstraintGuide(const JointDef &joint,const raggeometry::Pose &referenceParent,
    const raggeometry::Pose &referenceChild,const raggeometry::Pose &referenceChildBone,
    const raggeometry::Pose &currentParent,const raggeometry::Pose &currentChild,float radius,ConstraintGuide &out)
{
    using namespace raggeometry;
    if(!GuideRigid(referenceParent)||!GuideRigid(referenceChild)||!GuideRigid(referenceChildBone)||
        !GuideRigid(currentParent)||!GuideRigid(currentChild)||!std::isfinite(radius)||radius<=0||radius>1000000) return false;
    Pose local;
    if(joint.kind==JointKind::Hinge)
    {
        if(!GuideFinite(joint.hingeAxis)||!GuideFinite(joint.hingeAnchor)||Len(joint.hingeAxis)<1e-6||
            !std::isfinite(joint.hingeMin)||!std::isfinite(joint.hingeMax)||joint.hingeMin< -180||joint.hingeMax>180||joint.hingeMin>=joint.hingeMax) return false;
        const V x=Unit(joint.hingeAxis),seed=std::fabs(x.z)<.95?V{0,0,1}:V{0,1,0};
        local=Axis(x,seed);local.p=joint.hingeAnchor;
    }
    else if(joint.kind==JointKind::D6)
    {
        if(!GuideFinite(joint.frameAnchor)||!GuideFinite(joint.frameAngles)||!AngularMotionName(joint.twistMotion)||
            !AngularMotionName(joint.swing1Motion)||!AngularMotionName(joint.swing2Motion)||
            !std::isfinite(joint.twistLow)||!std::isfinite(joint.twistHigh)||!std::isfinite(joint.swing1)||!std::isfinite(joint.swing2)||
            joint.twistLow< -180||joint.twistHigh>180||joint.twistLow>=joint.twistHigh||joint.swing1<=0||joint.swing1>=180||joint.swing2<=0||joint.swing2>=180) return false;
        local=Euler(joint.frameAngles,joint.frameAnchor);
    }
    else return false;
    const Pose frame=Concat(referenceChildBone,local);ConstraintGuide result;
    result.parent=GuideBodyFrame(referenceParent,currentParent,frame);result.child=GuideBodyFrame(referenceChild,currentChild,frame);
    result.lines.push_back({result.parent.p,result.child.p,{1,1,.1f}});
    const V colors[]={{1,.2f,.2f},{.2f,1,.2f},{.3f,.5f,1}},axes[]={result.parent.x,result.parent.y,result.parent.z};
    for(int i=0;i<3;++i) result.lines.push_back({result.parent.p,Add(result.parent.p,Mul(axes[i],radius)),colors[i]});
    result.lines.push_back({result.child.p,Add(result.child.p,Mul(result.child.x,radius)),{.1f,1,1}});
    auto arc=[&](int axis,AngularMotion motion,float low,float high)
    {
        if(motion==AngularMotion::Locked) return;
        if(motion==AngularMotion::Free) {low=-180;high=180;}
        auto point=[&](double angle)->V
        {
            const float c=float(std::cos(angle)*radius),s=float(std::sin(angle)*radius);
            return result.parent.Point(axis==0?V{0,c,s}:axis==1?V{c,0,-s}:V{c,s,0});
        };
        V previous=point(low*pi/180);result.lines.push_back({result.parent.p,previous,colors[axis]});
        for(int sample=1;sample<=32;++sample)
        {
            const V next=point((low+(high-low)*sample/32.0)*pi/180);result.lines.push_back({previous,next,colors[axis]});previous=next;
        }
        result.lines.push_back({result.parent.p,previous,colors[axis]});
    };
    if(joint.kind==JointKind::Hinge) arc(0,AngularMotion::Limited,joint.hingeMin,joint.hingeMax);
    else {arc(0,joint.twistMotion,joint.twistLow,joint.twistHigh);arc(1,joint.swing1Motion,-joint.swing1,joint.swing1);arc(2,joint.swing2Motion,-joint.swing2,joint.swing2);}
    out=std::move(result);return true;
}
}
