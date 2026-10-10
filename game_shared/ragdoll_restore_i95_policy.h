#pragma once
#include <cmath>
#include <cstring>
#include <cstddef>

namespace physicsasset {
struct PivotXYZ { float x, y, z; };
inline PivotXYZ ScaleBonePivot(PivotXYZ pivot, PivotXYZ scale)
{
    return { pivot.x*scale.x, pivot.y*scale.y, pivot.z*scale.z };
}
// Require an entire bounded body snapshot, never partially restore a changed
// asset or an incomplete save. Caller checks stable per-body names/poses.
inline bool ValidRagdollSnapshotHeader(const char *savedModel, size_t savedModelBytes,
    const char *actualModel, int count, int capacity, int valid, int full, int frozen)
{
    return savedModel && savedModelBytes>0 &&
        std::memchr(savedModel,0,savedModelBytes)!=nullptr && savedModel[0] &&
        actualModel && std::strcmp(savedModel,actualModel)==0 && valid==1 &&
        count>0 && count<=capacity && (full==0 || full==1) &&
        (frozen==0 || frozen==1);
}
} // namespace physicsasset
