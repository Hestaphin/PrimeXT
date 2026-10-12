// Shared pxmv/client reconstruction after physical world-space bones are seeded.
#ifndef STUDIO_PHYSICAL_HIERARCHY_H
#define STUDIO_PHYSICAL_HIERARCHY_H

#include "physics_asset_render_coordinates.h"
#include <array>

namespace physicsasset
{
inline bool StudioUniformWorldBone(const matrix3x4 &bone, float scale)
{
    if(!std::isfinite(scale) || scale <= 1e-6f) return false;
    for(int column=0;column<4;++column)
        for(int row=0;row<3;++row)
            if(!std::isfinite(bone[column][row])) return false;
    const Vector f=bone.GetForward()/scale, r=bone.GetRight()/scale, u=bone.GetUp()/scale;
    return std::fabs(f.Length()-1.0f)<0.002f && std::fabs(r.Length()-1.0f)<0.002f &&
        std::fabs(u.Length()-1.0f)<0.002f && std::fabs(DotProduct(f,r))<0.002f &&
        std::fabs(DotProduct(f,u))<0.002f && std::fabs(DotProduct(r,u))<0.002f;
}

// rootBone >= 0 moves passive ancestors above a ragdoll root. rootBone == -1
// preserves animated ancestors for articulated parts. On failure world is
// untouched; callers can restore their animation snapshot transactionally.
template<class Bone>
inline bool StudioReconstructPhysicalHierarchy(int count, const Bone *bones,
    const matrix3x4 *animated, const bool *driven, int rootBone, float scale, matrix3x4 *world)
{
    constexpr int capacity=128; // Studio MAXSTUDIOBONES
    if(count<=0 || count>capacity || !bones || !animated || !driven || !world ||
        rootBone < -1 || rootBone >= count || (rootBone>=0 && !driven[rootBone])) return false;
    for(int i=0;i<count;++i)
        if(bones[i].parent < -1 || bones[i].parent >= count ||
            !StudioUniformWorldBone(animated[i],scale) ||
            (driven[i] && !StudioUniformWorldBone(world[i],scale))) return false;

    std::array<unsigned char,capacity> state{};
    std::array<int,capacity> path{},order{};
    int ordered=0;
    for(int i=0;i<count;++i)
    {
        if(state[i]==2) continue;
        int current=i, depth=0;
        while(current>=0 && state[current]!=2)
        {
            if(state[current]==1) return false; // cycle, including a self parent
            state[current]=1; path[depth++]=current; current=bones[current].parent;
        }
        while(depth>0) {current=path[--depth];state[current]=2;order[ordered++]=current;}
    }
    std::array<matrix3x4,capacity> result;
    std::array<bool,capacity> affected{};
    for(int i=0;i<count;++i) {result[i]=driven[i]?world[i]:animated[i];affected[i]=driven[i];}
    for(int child=rootBone;child>=0;)
    {
        const int parent=bones[child].parent;
        if(parent<0 || driven[parent]) break;
        const matrix3x4 local=StudioAnimatedWorldRelative(animated[parent],animated[child]);
        result[parent]=StudioRebuildPhysicalChild(result[child],local.Invert(),scale);
        affected[parent]=true; child=parent;
    }
    for(int p=0;p<ordered;++p)
    {
        const int i=order[p],parent=bones[i].parent;
        if(affected[i] || parent<0 || !affected[parent]) continue;
        const matrix3x4 local=StudioAnimatedWorldRelative(animated[parent],animated[i]);
        result[i]=StudioRebuildPhysicalChild(result[parent],local,scale);affected[i]=true;
    }
    for(int i=0;i<count;++i) if(!StudioUniformWorldBone(result[i],scale)) return false;
    for(int i=0;i<count;++i) world[i]=result[i];
    return true;
}
}
#endif
