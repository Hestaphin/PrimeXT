#pragma once

#include "ragdoll_diagnostics.h"
#include "physics_asset_render_coordinates.h"

namespace ragdolldiag
{
inline Pose StudioDiagnosticPose(const matrix3x4 &matrix)
{
    const Vector position = matrix.GetOrigin();
    const Vector4D rotation = physicsasset::StudioUnitWorldBone(matrix).GetQuaternion();
    return {{position.x,position.y,position.z}, {rotation.x,rotation.y,rotation.z,rotation.w}};
}

inline bool FiniteStudioMatrix(const matrix3x4 &matrix)
{
    const Vector columns[] = {matrix.GetForward(),matrix.GetRight(),matrix.GetUp(),matrix.GetOrigin()};
    for(const Vector &v : columns) if(!Finite({v.x,v.y,v.z})) return false;
    return matrix.GetForward().Length() > 1e-6f && matrix.GetRight().Length() > 1e-6f && matrix.GetUp().Length() > 1e-6f;
}

// Compare FINAL bone matrices with physical targets and independently compose
// the required animation-relative links. This never writes to any input.
// In the client, target delta also includes intentional snapshot interpolation.
// It is not a GPU mesh/vertex correctness certificate.
template<class Bone, class Mask>
inline PoseMetrics MeasureStudioPose(int count, const Bone *bones, const matrix3x4 *animated,
    const matrix3x4 *finalBones, const Mask &driven, const matrix3x4 *targets, int rootBone)
{
    PoseMetrics out;
    if(count <= 0 || !bones || !animated || !finalBones) return out;
    out.valid = true;
    out.bones = static_cast<unsigned>(count);
    const float scale = physicsasset::StudioPhysicalRenderScale(finalBones[0]);
    for(int i = 0; i < count; ++i)
    {
        if(!FiniteStudioMatrix(finalBones[i]) || !FiniteStudioMatrix(animated[i]))
        { ++out.invalidBones; continue; }
        const Pose actual = StudioDiagnosticPose(finalBones[i]);
        if(driven[i] && targets)
        {
            ++out.drivenCompared;
            if(!FiniteStudioMatrix(targets[i])) ++out.invalidComparisons;
            else ComparePose(out,actual,StudioDiagnosticPose(targets[i]),i,
                out.drivenPositionDelta,out.drivenAngleDelta,out.worstDrivenBone);
        }
        const int parent = bones[i].parent;
        if(parent < -1 || parent >= count || parent == i) { ++out.invalidParents; continue; }
        if(parent >= 0 && FiniteStudioMatrix(finalBones[parent]) && FiniteStudioMatrix(animated[parent]))
        {
            const double oldLength = Distance(StudioDiagnosticPose(animated[i]).position,StudioDiagnosticPose(animated[parent]).position);
            const double newLength = Distance(actual.position,StudioDiagnosticPose(finalBones[parent]).position);
            const double delta = std::fabs(newLength-oldLength);
            if(delta > out.edgeLengthDelta) { out.edgeLengthDelta = delta; out.worstEdgeBone = i; }
        }
        if(driven[i]) continue;

        int ancestor = parent, walked = 0;
        while(ancestor >= 0 && ancestor < count && !driven[ancestor] && walked++ < count)
            ancestor = bones[ancestor].parent;
        if(walked >= count || ancestor < -1 || ancestor >= count)
        { ++out.invalidParents; continue; }
        if(ancestor >= 0)
        {
            ++out.descendantsCompared;
            if(!FiniteStudioMatrix(animated[ancestor]) || !FiniteStudioMatrix(finalBones[ancestor]))
            { ++out.invalidComparisons; continue; }
            const matrix3x4 relative = physicsasset::StudioAnimatedWorldRelative(animated[ancestor],animated[i]);
            const matrix3x4 expected = physicsasset::StudioRebuildPhysicalChild(finalBones[ancestor],relative,scale);
            ComparePose(out,actual,StudioDiagnosticPose(expected),i,
                out.descendantPositionDelta,out.descendantAngleDelta,out.worstDescendantBone);
            continue;
        }
        // Non-driven ancestors above the physical root need the reverse link.
        // Stop at another driven ancestor: it owns its own physical transform.
        int child = rootBone;
        for(int guard = 0; guard < count && child >= 0 && child < count; ++guard)
        {
            child = bones[child].parent;
            if(child < 0 || child >= count || driven[child]) break;
            if(child != i) continue;
            ++out.ancestorsCompared;
            if(!FiniteStudioMatrix(animated[rootBone]) || !FiniteStudioMatrix(finalBones[rootBone]))
            { ++out.invalidComparisons; break; }
            const matrix3x4 relative = physicsasset::StudioAnimatedWorldRelative(animated[rootBone],animated[i]);
            const matrix3x4 expected = physicsasset::StudioRebuildPhysicalChild(finalBones[rootBone],relative,scale);
            ComparePose(out,actual,StudioDiagnosticPose(expected),i,
                out.ancestorPositionDelta,out.ancestorAngleDelta,out.worstAncestorBone);
            break;
        }
    }
    return out;
}
}
