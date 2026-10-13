#pragma once
#include "ragdoll_template.h"
#include <algorithm>
#include <utility>

namespace ragtemplate { namespace f5
{
inline Template HumanVariant(const Template &human,bool heavy)
{
    Template t=human;t.id=heavy?"human_heavy":"human_loose";t.name=heavy?"Human Heavy / Armored":"Human Loose";
    t.description=heavy?"Human anatomy with heavier mass and resistance; inspect armor clearance and live drops.":"Human anatomy with wider limits and lower resistance; translations stay locked; review live drops.";
    t.massBudget=heavy?110.0f:70.0f;t.simulation.linearDamping=heavy?.2f:.08f;t.simulation.angularDamping=heavy?.8f:.15f;
    t.simulation.angularResistance=heavy?3.0f:.4f;t.simulation.maxAngularVelocity=heavy?12.0f:25.0f;
    t.simulation.positionIterations=heavy?24:16;t.simulation.velocityIterations=heavy?8:4;
    if(!heavy) for(auto &r:t.roles)
    {
        r.joint.twistLow=std::max(-175.0f,r.joint.twistLow*1.15f);
        r.joint.twistHigh=std::min(175.0f,r.joint.twistHigh*1.15f);
        r.joint.swing1=std::min(175.0f,r.joint.swing1*1.15f);r.joint.swing2=std::min(175.0f,r.joint.swing2*1.15f);
    }
    return t;
}
inline Role MakeRole(const char *id,const char *parent,std::vector<std::string> aliases,
    const char *region,physicsasset::ShapeType shape,float weight,int detail=0,bool required=true)
{
    Role r;r.id=id;r.parent=parent;r.aliases=std::move(aliases);r.massRegion=region;r.shape=shape;r.massWeight=weight;
    r.minimumDetail=detail;r.required=required;r.joint.frameMode=FrameMode::Segment;
    const std::string area=region;
    r.hitGroup=r.id=="head"||r.id=="jaw"||r.id=="neck"?1:r.id=="pelvis"||r.id=="root"?3:r.id=="chest"?2:
        area=="left_front"||area=="left_limb"||area=="left_wing"?4:area=="right_front"||area=="right_limb"||area=="right_wing"?5:
        area=="left_hind"?6:area=="right_hind"?7:0;
    r.joint.twistLow=-25;r.joint.twistHigh=25;r.joint.swing1=35;r.joint.swing2=35;return r;
}
inline Template Quadruped()
{
    using S=physicsasset::ShapeType;Template t;t.id="quadruped";t.name="Quadruped / Animal";t.anatomy="quadruped";
    t.description="Four checked limb chains plus optional neck, feet, jaw and bounded tail. Review inferred flexion axes.";
    t.massBudget=40;t.simulation.angularDamping=.5f;t.simulation.angularResistance=1.5f;t.selfCollision=physicsasset::SelfCollisionMode::Selective;
    t.excludedSections={"weapon","gun","accessory"};
    t.massRegions={{"torso",.45f},{"head",.09f},{"left_front",.105f},{"right_front",.105f},{"left_hind",.105f},{"right_hind",.105f},{"tail",.04f}};
    t.roles.push_back(MakeRole("pelvis","",{"pelvis","hips","Bip01 Pelvis"},"torso",S::Box,4));
    t.roles.push_back(MakeRole("chest","pelvis",{"chest","spine2","Bip01 Spine2"},"torso",S::Box,6));
    t.roles.push_back(MakeRole("neck","chest",{"neck","Bip01 Neck"},"head",S::Capsule,1,1,false));
    t.roles.push_back(MakeRole("head","neck",{"head","Bip01 Head"},"head",S::Sphere,3));
    for(bool left:{true,false})for(bool front:{true,false})
    {
        const std::string side=left?"left":"right",end=front?"front":"hind",region=side+"_"+end;
        const std::string suffix=left?"l":"r",hl=left?"Bip01 L ":"Bip01 R ";
        const std::string upper=region+"_upper",lower=region+"_lower",foot=region+"_foot";
        t.roles.push_back(MakeRole(upper.c_str(),front?"chest":"pelvis",{end+"_upper_"+suffix,region+"_upper",hl+(front?"Arm":"Leg")},region.c_str(),S::Capsule,4));
        auto r=MakeRole(lower.c_str(),upper.c_str(),{end+"_lower_"+suffix,region+"_lower",hl+(front?"Arm1":"Leg1")},region.c_str(),S::Capsule,2);
        r.joint.frameMode=FrameMode::Flexion;r.joint.twistLow=0;r.joint.twistHigh=140;r.joint.swing1Motion=r.joint.swing2Motion=physicsasset::AngularMotion::Locked;t.roles.push_back(r);
        t.roles.push_back(MakeRole(foot.c_str(),lower.c_str(),{end+"_foot_"+suffix,region+"_foot",hl+(front?"Hand":"Foot")},region.c_str(),S::Box,1,1,false));
    }
    auto tail=MakeRole("tail","pelvis",{"tail","tail_01","Bip01 Tail1"},"tail",S::Capsule,1,1,false);tail.chainDepth=4;t.roles.push_back(tail);
    auto jaw=MakeRole("jaw","head",{"jaw","Bip01 Jaw"},"head",S::Capsule,.3f,2,false);jaw.joint.twistLow=-10;jaw.joint.twistHigh=35;t.roles.push_back(jaw);
    return t;
}
inline Template Creature()
{
    using S=physicsasset::ShapeType;Template t;t.id="creature";t.name="Creature / Monster";t.anatomy="creature";
    t.description="Editable core and optional limb/tail/wing seeds. Only declared descendants expand; unknown anatomy needs explicit mappings.";
    t.massBudget=90;t.simulation.angularDamping=.5f;t.simulation.angularResistance=1.5f;t.selfCollision=physicsasset::SelfCollisionMode::Selective;
    t.excludedSections={"weapon","gun","accessory"};
    t.massRegions={{"torso",.5f},{"head",.1f},{"left_limb",.1f},{"right_limb",.1f},{"tail",.1f},{"left_wing",.05f},{"right_wing",.05f}};
    t.roles.push_back(MakeRole("root","",{"root","pelvis","Bip01 Pelvis"},"torso",S::Box,5));
    t.roles.push_back(MakeRole("chest","root",{"chest","torso","Bip01 Spine3"},"torso",S::Box,5,0,false));
    t.roles.push_back(MakeRole("head","chest",{"head","Bip01 Head"},"head",S::Sphere,2,0,false));
    for(bool left:{true,false})
    {
        const std::string side=left?"left":"right",suffix=left?"l":"r",hl=left?"Bip01 L Arm1":"Bip01 R Arm1";
        auto limb=MakeRole((side+"_limb").c_str(),"chest",{"limb_"+suffix,"arm_"+suffix,hl},(side+"_limb").c_str(),S::Capsule,1,1,false);limb.chainDepth=3;t.roles.push_back(limb);
        auto wing=MakeRole((side+"_wing").c_str(),"chest",{"wing_"+suffix,side+"_wing"},(side+"_wing").c_str(),S::Capsule,1,2,false);wing.chainDepth=3;wing.chainBranches=true;t.roles.push_back(wing);
    }
    auto tail=MakeRole("tail","root",{"tail","tail_01","Bip01 Tail1"},"tail",S::Capsule,1,1,false);tail.chainDepth=6;t.roles.push_back(tail);
    auto jaw=MakeRole("jaw","head",{"jaw","Bip01 Jaw"},"head",S::Capsule,.3f,2,false);t.roles.push_back(jaw);
    return t;
}
}}
