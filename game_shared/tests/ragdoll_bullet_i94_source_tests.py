#!/usr/bin/env python3
from pathlib import Path
root = Path(__file__).resolve().parents[2]
files = {name: (root / name).read_text(encoding='latin1') for name in [
    'common/const.h', 'server/client.cpp', 'server/combat.cpp',
    'server/server_weapon_layer_impl.cpp','server/dll_int.cpp',
    'server/physx/physx_impl.cpp', 'server/physic.h','server/world.cpp',
]}
assert 'FTRACE_BULLET_PHYSICS' in files['common/const.h']
assert 'SkipStudioBulletHitbox' in files['server/client.cpp']
for name in ['server/combat.cpp','server/server_weapon_layer_impl.cpp']:
    assert 'SetBits(gpGlobals->trace_flags, FTRACE_MATERIAL_TRACE | FTRACE_BULLET_PHYSICS)' in files[name]
    assert 'ClearBits(gpGlobals->trace_flags, FTRACE_MATERIAL_TRACE | FTRACE_BULLET_PHYSICS)' in files[name]
assert 'GetRagdollWorldBounds(pEntity' in files['server/dll_int.cpp']
assert 'pev->mins/maxs intentionally stay' in files['server/dll_int.cpp']
assert 'ragdollquery::OverlapWorldBounds' in files['server/physx/physx_impl.cpp']
assert 'owner->pev->absmin, owner->pev->absmax' not in files['server/physx/physx_impl.cpp'].split('bool CPhysicPhysX::TraceBulletRagdolls(',1)[1].split('// Milestone I.5:',1)[0]
assert 'GetRagdollWorldBounds( CBaseEntity *pEntity' in files['server/physx/physx_impl.cpp']
assert 'virtual bool GetRagdollWorldBounds' in files['server/physic.h']
assert 'virtual bool GetRagdollWorldBounds' in files['server/world.cpp']
print('I.9.4 actual-source call-path assertions: PASS')
