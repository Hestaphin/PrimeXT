#pragma once
#include "studio.h"
#include "studio_pose_parameters.h"
#include "physics_part_net.h"
#include <cstddef>

// A settled physical pose does not make time-driven animation/IK/procedural
// bones static. Only models without those stages may reuse across samples.
inline bool StudioSleepingModelStable(const studiohdr_t *header)
{
    if(!header || header->length < static_cast<int>(sizeof(*header)) ||
        header->numbones <= 0 || header->numbones > MAXSTUDIOBONES || header->numseq <= 0) return false;
    const auto range = [&](int offset, int count, std::size_t size) {
        return offset >= 0 && offset <= header->length && count >= 0 &&
            static_cast<std::size_t>(count) <= static_cast<std::size_t>(header->length - offset) / size;
    };
    if(!range(header->boneindex, header->numbones, sizeof(mstudiobone_t)) ||
        !range(header->seqindex, header->numseq, sizeof(mstudioseqdesc_t))) return false;
    if(header->studiohdr2index)
    {
        if(!range(header->studiohdr2index, 1, sizeof(studiohdr2_t))) return false;
        const auto *extra = reinterpret_cast<const studiohdr2_t *>(reinterpret_cast<const byte *>(header) + header->studiohdr2index);
        if(extra->numikchains || extra->numikautoplaylocks) return false;
    }
    const auto *bones = reinterpret_cast<const mstudiobone_t *>(reinterpret_cast<const byte *>(header) + header->boneindex);
    for(int i = 0; i < header->numbones; ++i)
        if(bones[i].flags & (BONE_ALWAYS_PROCEDURAL | BONE_JIGGLE_PROCEDURAL)) return false;
    const auto *sequences = reinterpret_cast<const mstudioseqdesc_t *>(reinterpret_cast<const byte *>(header) + header->seqindex);
    for(int i = 0; i < header->numseq; ++i)
        if(sequences[i].flags & (STUDIO_AUTOPLAY | STUDIO_REALTIME)) return false;
    return true;
}

template<class Instance>
inline bool StudioCanReuseSleepingPose(const Instance &instance, const cl_entity_t &entity, float now)
{
    const float previous = instance.m_predictionCollisionStamp.sampleTime;
    if(!instance.m_cpuSleepingModelStable || !std::isfinite(previous) || now < previous ||
        !instance.m_bRagdollPose || !(instance.m_bRagdollSleeping || instance.m_bRagdollFrozen) ||
        !instance.m_bRagdollExitPoseValid || instance.m_bRagdollEntryBlendPending || instance.m_bRagdollExitBlend ||
        instance.m_bExternalBones || !(entity.curstate.iuser1 & CF_RAGDOLL) ||
        entity.curstate.solid != SOLID_NOT || entity.curstate.movetype != MOVETYPE_NONE ||
        entity.curstate.framerate != 0.0f || entity.curstate.gaitsequence != 0 ||
        partnet::AnyActivePartPose(instance.m_partPoseActive, MAXSTUDIOBONES)) return false;
    if(!std::isfinite(instance.m_flRagdollInterpDuration) || !std::isfinite(instance.m_flRagdollInterpStartTime)) return false;
    if(instance.m_flRagdollInterpDuration > 1e-6f &&
        (previous - instance.m_flRagdollInterpStartTime) / instance.m_flRagdollInterpDuration < 1.0f) return false;
    if(StudioPoseInterpolant(entity, now) != StudioPoseInterpolant(entity, previous)) return false;
    for(const auto &blend : instance.m_seqblend)
    {
        if(!std::isfinite(blend.blendtime) || !std::isfinite(blend.fadeout) || !std::isfinite(blend.cycle)) return false;
        if(blend.blendtime && blend.fadeout > 0 && blend.blendtime + blend.fadeout > previous) return false;
    }
    return true;
}
