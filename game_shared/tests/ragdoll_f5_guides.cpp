#include "physics_asset_constraint_guide.h"
#include "physics_d6_frame.h"
#include "physics_hinge_frame.h"
#include <iostream>
#include <limits>
using namespace physicsasset;using namespace physx;using namespace raggeometry;
static unsigned checks=0;
static void Check(bool v,const char *s){++checks;if(!v){std::cerr<<"FAIL "<<s<<'\n';std::abort();}}
static V VectorOf(PxVec3 p){return {p.x,p.y,p.z};}
static Pose PoseOf(PxTransform p){return {VectorOf(p.q.rotate({1,0,0})),VectorOf(p.q.rotate({0,1,0})),VectorOf(p.q.rotate({0,0,1})),VectorOf(p.p)};}
static double Error(const Pose &a,const Pose &b){return std::max({Len(Sub(a.p,b.p)),Len(Sub(a.x,b.x)),Len(Sub(a.y,b.y)),Len(Sub(a.z,b.z))});}
int main()
{
    const PxTransform parentRef(PxVec3(10,20,30),PxQuat(0,0,std::sin(.4f),std::cos(.4f))),childRef(PxVec3(20,25,40),PxQuat(std::sin(.3f),0,0,std::cos(.3f)));
    const PxTransform boneRef(PxVec3(17,23,37),PxQuat(0,std::sin(.5f),0,std::cos(.5f)));
    const PxTransform move(PxVec3(80,-30,14),PxQuat(0,0,std::sin(.7f),std::cos(.7f)));
    for(bool hinge:{false,true})
    {
        JointDef j;j.kind=hinge?JointKind::Hinge:JointKind::D6;j.frameAnchor={3,-2,5};j.frameAngles={31,-45,87};j.hingeAnchor={1,2,3};j.hingeAxis={0,1,1};
        const PxTransform frame=boneRef*(hinge?ChildHingeFrame(j):ChildD6Frame(j));
        const PxTransform parentCurrent=move*parentRef,childCurrent=move*childRef;
        const PxTransform expectedParent=parentCurrent*(parentRef.getInverse()*frame),expectedChild=childCurrent*(childRef.getInverse()*frame);
        ConstraintGuide guide;
        Check(BuildConstraintGuide(j,PoseOf(parentRef),PoseOf(childRef),PoseOf(boneRef),PoseOf(parentCurrent),PoseOf(childCurrent),4,guide),"finite joint guide generated");
        Check(Error(guide.parent,PoseOf(expectedParent))<.00004&&Error(guide.child,PoseOf(expectedChild))<.00004,"guide frames match shared runtime D6/hinge local frame construction");
        Check(Len(Sub(guide.parent.p,guide.child.p))<.00004,"coincident bind anchors remain coincident after common rigid motion");
        Check(guide.lines.size()==(hinge?39u:107u),"selected joint guides include axes and independent angular arcs");
        bool finite=true;for(const auto &line:guide.lines)finite=finite&&GuideFinite(line.from)&&GuideFinite(line.to)&&GuideFinite(line.color);
        Check(finite,"all overlay endpoints finite");
        const PxTransform separated(PxVec3(30,0,0),PxQuat());ConstraintGuide moved;
        Check(BuildConstraintGuide(j,PoseOf(parentRef),PoseOf(childRef),PoseOf(boneRef),PoseOf(parentCurrent),PoseOf(separated*childCurrent),4,moved)&&std::fabs(Len(Sub(moved.parent.p,moved.child.p))-30)<.0001,"anchor gap shows independent body displacement");
        const auto before=moved.lines.size();j.hingeAxis={0,0,0};j.frameAngles.x=std::numeric_limits<float>::quiet_NaN();
        Check(!BuildConstraintGuide(j,PoseOf(parentRef),PoseOf(childRef),PoseOf(boneRef),PoseOf(parentCurrent),PoseOf(childCurrent),4,moved)&&moved.lines.size()==before,"invalid frame leaves prior overlay unchanged");
    }
    JointDef free;free.twistMotion=free.swing1Motion=free.swing2Motion=AngularMotion::Free;ConstraintGuide g;
    const Pose identity;
    Check(BuildConstraintGuide(free,identity,identity,identity,identity,identity,4,g)&&g.lines.size()==107,"free axes draw full circles");
    free.twistMotion=free.swing1Motion=free.swing2Motion=AngularMotion::Locked;
    Check(BuildConstraintGuide(free,identity,identity,identity,identity,identity,4,g)&&g.lines.size()==5,"locked axes keep frame axes without angle arcs");
    Check(!BuildConstraintGuide(free,identity,identity,identity,identity,identity,0,g),"invalid guide radius rejected");
    Pose invalid=identity;invalid.x={2,0,0};Check(!BuildConstraintGuide(free,invalid,identity,identity,identity,identity,4,g),"non-rigid guide input rejected");
    std::cout<<"F5 D6/hinge guide frame/arc math: "<<checks<<" checks PASS; shared production SDK adapters, NO solver/GPU\n";
}
