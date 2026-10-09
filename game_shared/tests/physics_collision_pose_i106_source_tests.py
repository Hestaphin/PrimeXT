#!/usr/bin/env python3
from pathlib import Path
from pose_j10_6_2_integration import function
root = Path(__file__).resolve().parents[2]
draw = (root/'client/render/gl_studio_draw.cpp').read_text()
init = (root/'client/render/gl_studio_init.cpp').read_text()
utils = (root/'client/utils.cpp').read_text()
hdr = (root/'client/render/gl_studio.h').read_text()
getter=draw[draw.index('bool CStudioModelRenderer::StudioGetCollisionBonePose'):draw.index('int CStudioModelRenderer :: StudioGetBounds')]
# J.10.6.2 computes both consumers through the same CPU stages. No stale
# rendered-bone shortcut or pre-existing-instance prerequisite is permitted.
assert 'StudioEnsureCpuInstance(const_cast<cl_entity_t*>(e))' in getter
assert 'StudioEvaluatePredictionBones(e,inst,state,now)' in getter
assert 'world=inst.m_predictionCollisionBones[bone]' in getter
cpu=draw[draw.index('bool CStudioModelRenderer::StudioEvaluateCpuBones'):draw.index('bool CStudioModelRenderer::StudioEvaluatePredictionBones')]
assert 'CollisionPoseStateEqual(inst.m_predictionCollisionStamp.state,state)' in cpu
assert 'inst.m_predictionCollisionStamp.sampleTime==now' in cpu
assert 'inst.m_cpuPoseRoot==root' in cpu
assert 'StudioCollisionPoseState(e,' in getter
assert 'm_collisionPoseStamp' in hdr and 'm_collisionPoseStamp = physicsasset::CollisionPoseStamp{}' in init
start=draw.index('void CStudioModelRenderer :: AddStudioModelToDrawList')
end=draw.index('StudioCalcAttachments( m_pModelInstance->m_pbones )',start)
block=draw[start:end]
assert block.index('else StudioSetupBones( );') < block.index('StudioPublishCollisionPose();')
publisher=function(draw,'void CStudioModelRenderer::StudioPublishCollisionPose')
assert 'm_collisionPoseStamp.valid=false' in publisher
assert 'curstate.movetype==MOVETYPE_FOLLOW' in publisher
assert 'm_collisionPoseStamp=m_pModelInstance->m_predictionCollisionStamp' in publisher
for func in ('StudioSetBonesExternal', 'StudioSetPartPose', 'StudioSetRagdollPose'):
    start=draw.index('void CStudioModelRenderer :: '+func) if 'void CStudioModelRenderer :: '+func in draw else draw.index('void CStudioModelRenderer::'+func)
    next_fn=draw.find('\nvoid CStudioModelRenderer', start+5)
    assert 'm_collisionPoseStamp.valid = false' in draw[start:next_fn if next_fn>0 else None],func
assert 'R_StudioGetCollisionBonePose(ent,index,boneMatrix)' in utils
assert 'R_StudioGetCollisionBonePose(ent,primitive.boneIndex,boneMatrix)' in utils
print('I.10.6 renderer/prediction source-path checks passed')
