#!/usr/bin/env python3
"""I.9.2 source regression for the B4 ownership-gate wiring.
The policy C++ test verifies truth tables; this verifies actual callsites.
"""
from pathlib import Path

code = (Path(__file__).resolve().parents[2] / 'server/physx/physx_impl.cpp').read_text(encoding='latin1')

def method(start, end):
    return code.split(start, 1)[1].split(end, 1)[0]

bodygroup = method('void CPhysicPhysX::UpdateCollisionBodygroup(', 'void CPhysicPhysX::UpdatePhysicsAssetCollisionBodygroups(')
assert 'ApplyPhysicsAssetRootCollision( pEntity, state, true )' in bodygroup
assert 'ApplyPhysicsAssetRootCollision( pEntity, state, false )' in bodygroup
assert 'setFlag( PxShapeFlag::eSIMULATION_SHAPE, state.collisionRequested )' not in bodygroup
assert 'setFlag( PxShapeFlag::eSCENE_QUERY_SHAPE, state.collisionRequested )' not in bodygroup

ownership = method('void CPhysicPhysX::SetArticulatedCollisionOwnership(', 'void CPhysicPhysX::RebuildAnimatedCollisionTraceMesh(')
assert 'ApplyPhysicsAssetRootCollision( pEntity, state, true )' in ownership
assert 'SetAnimatedCollisionEnabled( state, state.collisionRequested )' in ownership

retracking = method('void CPhysicPhysX::TrackPhysicsAssetCollision(', 'void CPhysicPhysX::UpdateCollisionBodygroup(')
assert 'ApplyPhysicsAssetRootCollision( pObject, stored, false )' in retracking

toggle = method('void CPhysicPhysX::ToggleCollision(', 'void *CPhysicPhysX :: CreateBodyFromEntity(')
assert 'physicsasset::ResolveRootCollision(' in toggle

restore = method('void CPhysicPhysX::RestoreLegacyCollisionAfterRagdoll(', 'void CPhysicPhysX::SuspendEntityForRagdoll(')
assert 'ApplyPhysicsAssetRootCollision(pEntity, it->second,' in restore
assert 'savedRenderBody != it->second.renderBody' in restore
assert 'rootActorWasReplaced' in restore
assert restore.index('savedRenderBody = state.renderBodyAtSuspend') < restore.index('m_ragdollLegacyCollisionState.erase( stored )')
print('I.9.2 source ownership checks: PASS')
