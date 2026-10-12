#ifndef STUDIO_PHYSICAL_POSE_H
#define STUDIO_PHYSICAL_POSE_H

#include "physics_part_net.h"
#include "physics_asset_render_coordinates.h"
#include "studio_physical_hierarchy.h"

// Both an entity lifetime bit and a clear packet may end the same physics epoch.
// Only the first hand-off starts a blend; later clears preserve its source/time.
template<class Instance>
inline void StudioBeginRagdollExit(Instance& inst, float now)
{
    if (inst.m_bRagdollPose) {
        inst.m_bRagdollExitBlend=inst.m_bRagdollExitPoseValid;
        inst.m_flRagdollExitBlendStartTime=now;
        inst.m_flRagdollExitBlendDuration=0.18f;
    }
    inst.m_bRagdollPose=false;
    inst.m_bRagdollSleeping=false;
    inst.m_bRagdollFrozen=false;
    inst.m_bRagdollEntryBlendPending=false;
    inst.m_iRagdollRootBone=-1;
    inst.m_flRagdollInterpDuration=0.0f;
    memset(inst.m_ragdollBoneMask,0,sizeof(inst.m_ragdollBoneMask));
    inst.cached_frame=-1;
    inst.bonecache.frame=-999999.0f;
}

// One CPU reconstruction for drawing and prediction, including unsimulated
// ancestors, descendants, part overrides and transitions out of ragdoll mode.
template<class Instance>
inline void StudioApplyPartPosesCpu(Instance& inst, const studiohdr_t* hdr,
    const cl_entity_t& entity, const matrix3x4& root, float now, matrix3x4 worldBones[])
{
    if (inst.m_bRagdollPose || !hdr || !partnet::AnyActivePartPose(inst.m_partPoseActive,MAXSTUDIOBONES)) return;
    const int n=Q_min(hdr->numbones,MAXSTUDIOBONES);
    mstudiobone_t *bones=(mstudiobone_t *)((byte *)hdr+hdr->boneindex);
    matrix3x4 animated[MAXSTUDIOBONES];
    const float renderScale = physicsasset::StudioPhysicalRenderScale(root);
    bool changed[MAXSTUDIOBONES]={false};
    bool anyChanged=false;
    for (int i=0;i<n;++i) animated[i]=worldBones[i];
    for (int i=0;i<n;++i) {
        if (!inst.m_partPoseActive[i]) continue;
        // Prevent stale PVS/out-of-order snapshots from pinning a limb forever.
        if (!partnet::PartPoseFresh(now, inst.m_partPoseReceived[i])) {
            inst.m_partPoseActive[i]=false;
            inst.m_partPosePending[i]=false;
            continue;
        }
        if (inst.m_partPosePending[i]) {
            animated[i].GetOrigin(inst.m_partPoseStartPos[i]);
            inst.m_partPoseStartQuat[i]=physicsasset::StudioUnitWorldBone(animated[i]).GetQuaternion();
            inst.m_partPosePending[i]=false;
            // Receipt-time capture failed. Start when this source actually
            // exists, so a delayed first query/draw cannot skip the entry blend.
            inst.m_partPoseStartTime[i]=now;
        }
        const float t=bound(0.0f,(now-inst.m_partPoseStartTime[i])/
            Q_max(0.001f,inst.m_partPoseDuration[i]),1.0f);
        Vector4D q;
        QuaternionSlerp(inst.m_partPoseStartQuat[i],inst.m_partPoseTargetQuat[i],t,q);
        const Vector pos=inst.m_partPoseStartPos[i]*(1.0f-t)+
            inst.m_partPoseTargetPos[i]*t;
        worldBones[i]=physicsasset::StudioScaledWorldBone(pos,q,renderScale);
        changed[i]=true; anyChanged=true;
    }
    if(anyChanged && !physicsasset::StudioReconstructPhysicalHierarchy(n,bones,animated,changed,-1,renderScale,worldBones))
        for(int i=0;i<n;++i) worldBones[i]=animated[i];
}

template<class Instance>
inline void StudioApplyRagdollPoseCpu(Instance& inst, const studiohdr_t* hdr,
    const cl_entity_t& entity, const matrix3x4& root, float now, matrix3x4 worldBones[])
{
	if( !hdr ) return;
	const float renderScale = physicsasset::StudioPhysicalRenderScale(root);

	// After physics ownership ends, blend the cached final world-space skeleton
	// into the freshly evaluated animation skeleton. This is deliberately done
	// after normal animation/IK so the destination is the exact current pose.
	if( !inst.m_bRagdollPose )
	{
		if( !inst.m_bRagdollExitBlend || !inst.m_bRagdollExitPoseValid ) return;
		const float duration = Q_max( 0.001f, inst.m_flRagdollExitBlendDuration );
		const float alpha = bound( 0.0f, ( now - inst.m_flRagdollExitBlendStartTime ) / duration, 1.0f );
		const int boneCount = Q_min( hdr->numbones, MAXSTUDIOBONES );
		for( int i = 0; i < boneCount; ++i )
		{
			Vector fromPos, toPos;
			inst.m_ragdollExitPose[i].GetOrigin( fromPos );
			worldBones[i].GetOrigin( toPos );
			Vector4D fromQ = physicsasset::StudioUnitWorldBone(inst.m_ragdollExitPose[i]).GetQuaternion();
			Vector4D toQ = physicsasset::StudioUnitWorldBone(worldBones[i]).GetQuaternion();
			Vector4D outQ;
			QuaternionSlerp( fromQ, toQ, alpha, outQ );
			worldBones[i] = physicsasset::StudioScaledWorldBone( fromPos * ( 1.0f - alpha ) + toPos * alpha, outQ, renderScale );
		}
		if( alpha >= 1.0f )
		{
			inst.m_bRagdollExitBlend = false;
			inst.m_bRagdollExitPoseValid = false;
		}
		return;
	}

	// Reconstruct at this client sample before handing ownership back. The last
	// cached pose may predate packet interpolation or an offscreen interval.
	const bool endingPhysics = !FBitSet( entity.curstate.iuser1, CF_RAGDOLL );

	mstudiobone_t *pbones = (mstudiobone_t *)((byte *)hdr + hdr->boneindex);
	matrix3x4 animated[MAXSTUDIOBONES];
	const int boneCount = Q_min( hdr->numbones, MAXSTUDIOBONES );
	const int rootBone = inst.m_iRagdollRootBone;
	if( rootBone < 0 || rootBone >= boneCount || !inst.m_ragdollBoneMask[rootBone] )
	{
		if( endingPhysics ) StudioBeginRagdollExit(inst,now);
		return;
	}

	for( int i = 0; i < boneCount; ++i )
		animated[i] = worldBones[i];

	if( inst.m_bRagdollEntryBlendPending )
	{
		// If packet-time evaluation was unavailable, either CPU consumer captures
		// its freshly evaluated animation skeleton instead of stale draw bones.
		matrix3x4 animatedRoot = animated[rootBone];
		animatedRoot.GetOrigin( inst.m_ragdollStartRootOrigin );
		inst.m_ragdollStartRootQuat = physicsasset::StudioUnitWorldBone(animatedRoot).GetQuaternion();
		const matrix3x4 rootInverse = physicsasset::StudioUnitWorldBone(animatedRoot).Invert();
		for( int i = 0; i < boneCount; ++i )
		{
			if( i == rootBone || !inst.m_ragdollBoneMask[i] ) continue;
			matrix3x4 relative = rootInverse.ConcatTransforms( physicsasset::StudioUnitWorldBone(animated[i]) );
			relative.GetOrigin( inst.m_ragdollStartRelOrigin[i] );
			inst.m_ragdollStartRelQuat[i] = relative.GetQuaternion();
		}
		inst.m_flRagdollInterpStartTime = now;
		inst.m_flRagdollInterpDuration = 0.12f;
		inst.m_bRagdollEntryBlendPending = false;
	}

	float alpha = 1.0f;
	if( inst.m_flRagdollInterpDuration > 1e-6f )
		alpha = bound( 0.0f, ( now - inst.m_flRagdollInterpStartTime ) / inst.m_flRagdollInterpDuration, 1.0f );

	Vector rootPos = inst.m_ragdollStartRootOrigin * ( 1.0f - alpha ) + inst.m_ragdollTargetRootOrigin * alpha;
	Vector4D rootQ;
	QuaternionSlerp( inst.m_ragdollStartRootQuat, inst.m_ragdollTargetRootQuat, alpha, rootQ );
	const matrix3x4 rootWorld( rootPos, rootQ );
	worldBones[rootBone] = physicsasset::StudioScaledWorldBone(rootPos,rootQ,renderScale);

	// Interpolate the articulated pose in root space rather than independently in
	// world space. This preserves the skeleton's relative structure while the root
	// translates/rotates between snapshots.
	for( int i = 0; i < boneCount; ++i )
	{
		if( i == rootBone || !inst.m_ragdollBoneMask[i] ) continue;
		const Vector relPos = inst.m_ragdollStartRelOrigin[i] * ( 1.0f - alpha ) + inst.m_ragdollTargetRelOrigin[i] * alpha;
		Vector4D relQ;
		QuaternionSlerp( inst.m_ragdollStartRelQuat[i], inst.m_ragdollTargetRelQuat[i], alpha, relQ );
		matrix3x4 bodyWorld = rootWorld.ConcatTransforms( matrix3x4( relPos, relQ ));
		worldBones[i] = physicsasset::StudioScaledWorldBone(bodyWorld.GetOrigin(),bodyWorld.GetQuaternion(),renderScale);
	}

    if(!physicsasset::StudioReconstructPhysicalHierarchy(boneCount,pbones,animated,
        inst.m_ragdollBoneMask,rootBone,renderScale,worldBones))
    {
        for(int i=0;i<boneCount;++i) worldBones[i]=animated[i];
        if(endingPhysics) StudioBeginRagdollExit(inst,now);
        return;
    }

	for( int i = 0; i < boneCount; ++i )
		inst.m_ragdollExitPose[i] = worldBones[i];
	inst.m_bRagdollExitPoseValid = true;
	if( endingPhysics ) StudioBeginRagdollExit(inst,now);
}

#endif
