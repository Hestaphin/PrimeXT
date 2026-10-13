#pragma once
// Synthetic analytic skeletons, not redistributable model/game acceptance data.
#include "ragdoll_template.h"
inline int F5Bone(std::vector<ragtemplate::Bone> &bones,const std::string &name,int parent,physicsasset::Vec3 origin)
{
    ragtemplate::Bone b;b.name=name;b.parent=parent;b.localOrigin=origin;b.hasBounds=true;
    b.boundsMin={-1,-1,-1};b.boundsMax={3,1,1};
    for(int corner=0;corner<8;++corner)b.vertices.push_back({corner&1?3.0f:-1.0f,corner&2?1.0f:-1.0f,corner&4?1.0f:-1.0f});
    const int index=int(bones.size());bones.push_back(b);if(parent>=0)bones[parent].childOffset=origin;return index;
}
inline std::vector<ragtemplate::Bone> F5Quadruped()
{
    std::vector<ragtemplate::Bone> b;const int pelvis=F5Bone(b,"pelvis",-1,{0,0,30});
    const int chest=F5Bone(b,"chest",pelvis,{20,0,3}),neck=F5Bone(b,"neck",chest,{8,0,4}),head=F5Bone(b,"head",neck,{5,0,2});
    for(bool left:{true,false})for(bool front:{true,false})
    {
        const std::string e=front?"front":"hind",s=left?"l":"r";
        const int upper=F5Bone(b,e+"_upper_"+s,front?chest:pelvis,{0,left?8.0f:-8.0f,-3});
        const int lower=F5Bone(b,e+"_lower_"+s,upper,{0,0,-12});F5Bone(b,e+"_foot_"+s,lower,{2,0,-10});
    }
    int tail=F5Bone(b,"tail",pelvis,{-6,0,0});for(int i=2;i<=5;++i)tail=F5Bone(b,"tail_0"+std::to_string(i),tail,{-5,0,1});
    F5Bone(b,"jaw",head,{3,0,-2});return b;
}
inline std::vector<ragtemplate::Bone> F5Creature()
{
    std::vector<ragtemplate::Bone> b;const int root=F5Bone(b,"root",-1,{0,0,25}),chest=F5Bone(b,"chest",root,{0,0,15}),head=F5Bone(b,"head",chest,{0,0,10});
    for(bool left:{true,false})
    {
        const std::string s=left?"l":"r";
        const int limb=F5Bone(b,"limb_"+s,chest,{left?8.0f:-8.0f,0,0}),lower=F5Bone(b,"lower_"+s,limb,{left?9.0f:-9.0f,0,-3});F5Bone(b,"tip_"+s,lower,{left?7.0f:-7.0f,0,0});
        const int wing=F5Bone(b,"wing_"+s,chest,{left?7.0f:-7.0f,-3,5});
        for(char branch:{'a','b'})
        {
            const int feather=F5Bone(b,"feather_"+s+"_"+branch,wing,{left?8.0f:-8.0f,branch=='a'?5.0f:-5.0f,2});
            F5Bone(b,"feather_tip_"+s+"_"+branch,feather,{left?7.0f:-7.0f,0,2});
        }
    }
    int tail=F5Bone(b,"tail",root,{0,-5,-3});for(int i=2;i<=4;++i)tail=F5Bone(b,"tail_0"+std::to_string(i),tail,{0,-7,-1});
    F5Bone(b,"jaw",head,{0,4,-2});return b;
}
