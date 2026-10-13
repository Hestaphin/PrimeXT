#!/usr/bin/env python3
"""Source wiring checks for I.10.5, independent of the PhysX dependency."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
renderer = (root / "client/render/gl_studio_draw.cpp").read_text()
shared = (root / "game_shared/physics_part_net.h").read_text()
cache_fn = renderer.split("bool CStudioModelRenderer :: CheckBoneCache( float f )", 1)[1].split("void CStudioModelRenderer :: StudioSetBonesExternal", 1)[0]
assert "partnet::AnyActivePartPose(inst->m_partPoseActive, MAXSTUDIOBONES)" in cache_fn
assert '!partnet::AnyActivePartPose' in cache_fn
assert 'const bool cacheHit = canCache' in cache_fn
assert cache_fn.index('CopyToArray( inst->m_glmatrix )') < cache_fn.index('return cacheHit;')
setup = renderer.split("void CStudioModelRenderer::StudioSetupBones(void)", 1)[1].split("// convert bones into compacted GLSL array", 1)[0]
assert 'StudioEvaluateCpuBones' in setup and 'm_predictionCollisionBones[i]' in setup
cpu = renderer.split('bool CStudioModelRenderer::StudioEvaluateCpuBones',1)[1].split('bool CStudioModelRenderer::StudioEvaluatePredictionBones',1)[0]
assert cpu.index('StudioApplyRagdollPoseCpu') < cpu.index('StudioApplyPartPosesCpu')
apply = (root/'game_shared/studio_physical_pose.h').read_text()
assert "partnet::PartPoseFresh(now, inst.m_partPoseReceived[i])" in apply
assert "inst.m_partPoseActive[i]=false;" in apply
assert "inst.m_partPosePending[i]=false;" in apply
assert "partnet::PartPoseFresh(now, previousReceived)" in renderer
assert "now >= received" in shared and "std::isfinite(now)" in shared
assert "StudioApplyPartPosesCpu(inst,hdr,*e,root,now,world);" in cpu
print("I.10.5 renderer cache bypass, expiry and interpolation wiring PASS")
