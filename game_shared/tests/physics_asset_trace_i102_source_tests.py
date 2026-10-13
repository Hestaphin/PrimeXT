#!/usr/bin/env python3
"""I.10.2 source wiring assertions (not a game/PhysX runtime test)."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
sv=(root/'server/physx/physx_impl.cpp').read_text()
cl=(root/'client/utils.cpp').read_text()
helper=(root/'game_shared/physics_asset_trace_coordinates.h').read_text()
assert '#include "physics_asset_trace_coordinates.h"' in sv
assert '#include "physics_asset_trace_coordinates.h"' in cl
server=sv[sv.index('void CPhysicPhysX::RebuildAnimatedCollisionTraceMesh('):sv.index('void CPhysicPhysX::BuildAnimatedCollisionActors(')]
client=cl[cl.index('static bool BuildPredictedPhysicsMesh('):cl.index('void Physic_SweepTest(')]
assert 'traceBone.Initialize( entityToWorld, boneToWorld, modelScale )' in server
assert 'performance::FindTracePose(state.traceCandidate' in server
key=sv[sv.index('void CPhysicPhysX::BuildAnimatedTraceKey('):sv.index('void CPhysicPhysX::RebuildAnimatedCollisionTraceMesh(')]
assert 'StudioBoneWorldTransform(entity, bone)' in key
assert 'MatrixFromPxTransform(part.actor->getGlobalPose())' in key
assert 'traceBone.ToTraceLocal( local )' in server
assert 'traceBone.Initialize(entityToWorld,boneToWorld,modelScale)' in client
assert 'traceBone.ToTraceLocal(point)' in client
assert 'BuildPredictedPhysicsMesh(pTouch, *asset, physScale, physicsMesh)' in cl
assert 'Vector(state.scale) : Vector(1,1,1)' in cl
assert 'entityFrame.VectorITransform(world)' in helper
assert 'localScaled.x / scale.x' in helper
assert 'forward * (boneLocal.x * scale.x)' in helper
assert 'right * (boneLocal.y * scale.y)' in helper
assert 'up * (boneLocal.z * scale.z)' in helper
assert 'SetMeshOrientation( pTouch->pev->origin, pTouch->pev->angles, pTouch->GetScale() )' in sv
assert 'predictionTrace.SetMeshOrientation(state.origin,state.angles,physScale)' in cl
print('I.10.2 scaled-trace source wiring PASS')
