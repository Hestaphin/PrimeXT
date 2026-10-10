#!/usr/bin/env python3
"""I.10.9 source wiring guards; does not execute PhysX or editor GUI."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
cpp = (root/'server/physx/physx_impl.cpp').read_text()
editor = (root/'utils/pxmv/PhysicsAssetEditor.cpp').read_text()
policy = (root/'game_shared/physics_part_restore_policy.h').read_text()
restore = cpp.split('void CPhysicPhysX::RestoreInteractiveParts(',1)[1].split('\nbool CPhysicPhysX::DamagePhysicsPart(',1)[0]
apply_body = editor.split('const int requiredDimensions =',1)[1].split('    if(!ReadStrictFloat(m_mass',1)[0]
assert '#include "physics_part_restore_policy.h"' in cpp
assert 'CanRestoreInteractivePart(setup, saved.mode, saved.detached)' in restore
assert 'ValidRestoredPartSnapshot(saved.damage,' in restore
assert restore.index('CanRestoreInteractivePart(setup, saved.mode, saved.detached)') < restore.index('setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false)')
assert restore.index('pose.isValid()') < restore.index('setGlobalPose(pose)')
assert 'RigidUniformScale(' in restore
assert 'state->second.articulatedOverride' in restore
assert 'std::find(state->second.hinges.begin()' in restore
assert 'parent && parent != part->actor && child == part->actor' in restore
assert 'part->hinge->setDriveForceLimit(' in restore
assert 'part->sampledPose = false' in restore
assert 'savedDetached' in policy and 'savedMode == 1' in policy
assert 'ShapeType::Capsule && axis == 1' in apply_body
assert 'dimension[axis] < 0.0f' in apply_body
assert 'dimension[axis] <= 0.0f' not in apply_body
print('I.10.9 restore transition guards and editor capsule validation source wiring PASS')
