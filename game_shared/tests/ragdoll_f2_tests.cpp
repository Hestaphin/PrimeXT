#include "ragdoll_template.h"
#include "ragdoll_geometry.h"
#include "physics_asset_scale.h"
#include "fixtures/j10_6_10_1/barney_skeleton.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace ragtemplate;
using namespace raggeometry;
static int checks=0;
static void Check(bool b,const char *name) {++checks;if(!b) {std::cerr<<"FAIL "<<name<<'\n';std::abort();}}
static bool Near(double a,double b) {return std::fabs(a-b)<0.0002;}
static const physicsasset::BodyDef &Body(const Candidate &c,const char *name) {return c.asset.bodies.at(physicsasset::FindBody(c.asset,name));}
static const physicsasset::JointDef &Joint(const Candidate &c,const char *child) {for(const auto &j:c.asset.joints) if(j.child==child) return j;std::abort();}
int main()
{
    physicsasset::AssetDef empty;std::string error;Candidate c,d,s;
    Check(Generate(Builtins()[0],BarneySkeleton(),1,empty,ComposeMode::ReplaceRagdoll,c,&error),"recorded Barney geometry generates");
    Check(c.asset.bodies.size()==15&&c.asset.joints.size()==14&&c.axisReviews==4,"simple substantial standard rig with explicit axis review");
    Check(Generate(Builtins()[0],BarneySkeleton(),2,empty,ComposeMode::ReplaceRagdoll,d,&error)&&d.asset.bodies.size()==18,"detailed rig merges empty intermediate masses");
    Check(Generate(Builtins()[0],BarneySkeleton(),0,empty,ComposeMode::ReplaceRagdoll,s,&error)&&s.asset.bodies.size()==11,"simple rig omits optional segments");
    for(const auto *rig:{&c,&d,&s})
    {
        double total=0;for(const auto &body:rig->asset.bodies) total+=body.mass;
        Check(Near(total,70),"budget preserved at every detail");
        double left=0,right=0;for(const auto &body:rig->asset.bodies) {if(body.hitGroup==4) left+=body.mass;if(body.hitGroup==5) right+=body.mass;}
        Check(Near(left,5.6)&&Near(right,5.6),"regional limb budget survives missing hands/shoulders");
        Check(Near(Body(*rig,"Bip01 Head").mass,5.6),"head mass independent of empty neck links");
        Check(rig->massRatioWarnings==0,"default neighbor mass ratios within limit");
        Check(physicsasset::FindBody(rig->asset,"Bone03")<0&&physicsasset::FindBody(rig->asset,"Bone05")<0,"attachments excluded by anatomy");
    }
    auto intermediate=BarneySkeleton();intermediate[10].boundsMax.x+=20;Candidate pooled;
    Check(Generate(Builtins()[0],intermediate,1,empty,ComposeMode::ReplaceRagdoll,pooled,&error)&&Body(pooled,"Bip01 Pelvis").shape.halfExtents.x>Body(c,"Bip01 Pelvis").shape.halfExtents.x+5,"unnamed intermediate torso vertices pooled into the physical support body");
    intermediate=BarneySkeleton();intermediate[23].boundsMax.x=9999;
    Check(Generate(Builtins()[0],intermediate,1,empty,ComposeMode::ReplaceRagdoll,pooled,&error)&&Body(pooled,"Bip01 Pelvis").shape.halfExtents.x==Body(c,"Bip01 Pelvis").shape.halfExtents.x,"sibling accessory vertices never pooled into torso");
    Check(Joint(c,"Bip01 L Arm").parent=="Bip01 Spine3"&&Joint(c,"Bip01 L Arm1").parent=="Bip01 L Arm","left shoulder supported by chest");
    Check(Joint(c,"Bip01 R Arm").parent=="Bip01 Spine3"&&Joint(c,"Bip01 R Arm1").parent=="Bip01 R Arm","right shoulder supported by chest");
    for(const char *child:{"Bip01 L Arm2","Bip01 R Arm2","Bip01 L Leg1","Bip01 R Leg1"})
    {
        const auto &j=Joint(c,child);
        Check(j.twistLow==0&&j.twistHigh>=130&&j.swing1Motion==physicsasset::AngularMotion::Locked&&j.swing2Motion==physicsasset::AngularMotion::Locked,"asymmetric elbow/knee with secondary axes locked");
        Check(Len(Euler(j.frameAngles).x)>0.999,"valid oriented flexion frame");
    }
    Candidate free;Check(Generate(Builtins()[1],BarneySkeleton(),1,empty,ComposeMode::ReplaceRagdoll,free,&error),"free rig");
    Check(free.axisReviews==0,"Free Rotation requires no irrelevant flexion sign approval");
    for(const auto &j:free.asset.joints) Check(j.twistMotion==physicsasset::AngularMotion::Free&&j.swing1Motion==physicsasset::AngularMotion::Free&&j.swing2Motion==physicsasset::AngularMotion::Free,"all angular DOFs free");
    auto model=BarneySkeleton();model[3].localAngles={0.1f,0.2f,0.3f};model[2].childOffset={6,8,0};
    // A model-independent custom three-link chain exercises diagonal fitting.
    auto custom=Builtins()[2];custom.roles[0].aliases={"root"};Role limb;limb.id="limb";limb.parent="root";limb.aliases={"limb"};limb.joint.frameMode=FrameMode::Flexion;limb.joint.swing1Motion=limb.joint.swing2Motion=physicsasset::AngularMotion::Locked;limb.joint.twistLow=0;limb.joint.twistHigh=130;custom.roles.push_back(limb);
    Bone root;root.name="root";Bone bone;bone.name="limb";bone.parent=0;bone.localOrigin={3,0,0};bone.childOffset={6,8,0};bone.localAngles={0,0,0.6f};
    for(int i=0;i<40;++i) bone.vertices.push_back(Add(Mul(bone.childOffset,(i%10)/9.0),{0,0,i%2?1.0f:-1.0f}));
    bone.vertices.push_back({9999,9999,9999});
    Candidate diagonal;Check(Generate(custom,{root,bone},1,empty,ComposeMode::ReplaceRagdoll,diagonal,&error),"diagonal segmented custom skeleton");
    const auto &fitted=diagonal.asset.bodies[1];
    Check(Dot(Euler(fitted.angles).x,Unit(bone.childOffset))>0.9999,"capsule X aligned with actual limb axis");
    Check(fitted.shape.radius<=2.2001&&fitted.shape.radius<1.2,"radius capped and robust to a single extreme outlier");
    Check(Near(fitted.shape.halfHeight+fitted.shape.radius,4.4),"end trimming respects actual length");
    auto reversed=custom;reversed.roles[1].joint.flexionSign=-1;Candidate opposite;
    Check(Generate(reversed,{root,bone},1,empty,ComposeMode::ReplaceRagdoll,opposite,&error)&&Dot(Euler(opposite.asset.joints[0].frameAngles).x,Euler(diagonal.asset.joints[0].frameAngles).x)<-0.999,"explicit mirrored flexion sign reverses physical axis");
    custom.roles[1].joint.frameMode=FrameMode::Explicit;custom.roles[1].joint.frameAnchor={1,2,3};custom.roles[1].joint.frameAngles={10,20,30};
    Check(Generate(custom,{root,bone},1,empty,ComposeMode::ReplaceRagdoll,diagonal,&error)&&diagonal.axisReviews==0&&diagonal.asset.joints[0].frameAngles.y==20,"explicit reviewed frames bypass inference");
    physicsasset::AssetDef restored;Check(physicsasset::Parse(physicsasset::Serialize(diagonal.asset),restored,&error)&&physicsasset::Serialize(restored)==physicsasset::Serialize(diagonal.asset),"frames persist in self-contained runtime asset");
    Check(physicsasset::ScaleAuthoredAsset(restored,2)&&restored.joints[0].frameAnchor.x==2&&restored.joints[0].frameAngles.z==30,"frame anchor scaled once and angles unchanged");
    restored.joints[0].frameAngles.x=std::numeric_limits<float>::quiet_NaN();Check(!physicsasset::Validate(restored,&error),"nonfinite runtime frame rejected");
    Template parsed;Check(Parse(Serialize(custom),parsed,&error)&&Serialize(parsed)==Serialize(custom),"F2 template fields round trip");
    auto invalid=Builtins()[0];invalid.roles[0].massRegion="missing";Check(!Validate(invalid,&error),"missing mass region rejected");
    invalid=Builtins()[0];invalid.radiusRatio=0.9;Check(!Validate(invalid,&error),"oversize radius ratio rejected");
    invalid=Builtins()[0];invalid.roles[0].joint.flexionSign=0;Check(!Validate(invalid,&error),"zero flexion sign rejected");
    invalid=Builtins()[0];invalid.massRegions.push_back(invalid.massRegions[0]);Check(!Validate(invalid,&error),"duplicate region rejected");
    auto heavy=c.asset;heavy.bodies[0].mass=10000;Candidate merged;
    Check(Generate(Builtins()[0],BarneySkeleton(),1,heavy,ComposeMode::Merge,merged,&error)&&merged.massRatioWarnings>0&&physicsasset::Serialize(merged.asset)==physicsasset::Serialize(heavy),"authored excessive neighbor ratio reported without mutation");
    physicsasset::BodyDef a,b;a.shape.type=b.shape.type=physicsasset::ShapeType::Box;a.shape.halfExtents=b.shape.halfExtents={1,1,1};
    b.offset={1.5f,0,0};Check(Near(Overlap(a,{},b,{}),0.5),"analytic box overlap");b.offset={3,0,0};Check(Overlap(a,{},b,{})==0,"separated boxes");
    a.shape.type=b.shape.type=physicsasset::ShapeType::Capsule;a.shape.radius=b.shape.radius=1;a.shape.halfHeight=b.shape.halfHeight=2;b.offset={0,1.5f,0};
    Check(Near(Overlap(a,{},b,{}),0.5)&&Near(Overlap(a,{},b,{}),Overlap(b,{},a,{})),"capsule overlap symmetric");
    b.shape.type=physicsasset::ShapeType::Box;b.shape.halfExtents={1,1,1};b.offset={0,1.5f,0};Check(Near(Overlap(a,{},b,{}),0.5),"capsule/OBB distance");
    Check(Near(SegmentBox({-3,2,0},{3,2,0},{1,1,1}),1),"segment/AABB closest interval");
    b.shape.type=physicsasset::ShapeType::Sphere;b.shape.radius=1;b.offset={0,1.5f,0};Check(Near(Overlap(a,{},b,{}),0.5),"sphere/capsule overlap");
    for(V angle:{V{30,60,-45},V{90,20,70},V{-90,-15,40}})
    {const auto p=Euler(angle);const auto q=Euler(Angles(p));Check(Dot(p.x,q.x)>0.99999&&Dot(p.y,q.y)>0.99999&&Dot(p.z,q.z)>0.99999,"Euler frame reconstruction including gimbal positions");}
    std::cout<<"F2 anatomy/mass/fitting/frames/overlap: "<<checks<<" checks PASS (NO solver/GUI/game)\n";
}
