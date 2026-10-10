#!/usr/bin/env python3
"""Run unchanged production death, weight, impulse and shape-query methods.

Fixtures adapt entity services and PhysX actor/query calls explicitly. They do
not supply SDK headers or certify PhysX linking, contacts or solver behavior.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from pose_j10_6_2_integration import function

ROOT=Path(__file__).resolve().parents[2]

def main():
    asset=(ROOT/'server/physx/physics_asset.cpp').read_text()
    physics=(ROOT/'server/physx/physx_impl.cpp').read_text()
    combat=(ROOT/'server/combat.cpp').read_text()
    weapons=(ROOT/'server/weapons.cpp').read_text()
    methods=[function(asset,s) for s in [
        'bool PhysicsAssetInstance::NeedsAnimationControl',
        'void PhysicsAssetInstance::SetBodyKinematicState',
        'bool PhysicsAssetInstance::IsBodyDescendantOf',
        'bool PhysicsAssetInstance::SetBodyPhysicsWeight',
        'void PhysicsAssetInstance::ApplyAnimationControl',
        'bool PhysicsAssetInstance::AddImpulseToBody',
        'bool PhysicsAssetInstance::SetRagdoll',
    ]]
    methods += [function(physics,s) for s in [
        'void CPhysicPhysX::SuspendEntityForRagdoll',
        'bool CPhysicPhysX::SetRagdollBodyPhysicsWeight',
        'void CPhysicPhysX::TransferPendingRagdollImpulses',
        'void CPhysicPhysX::SetRagdoll(',
        'void CPhysicPhysX::QueuePhysicsAssetImpulse',
        'bool CPhysicPhysX::TraceBallisticPhysicsExit',
    ]]
    methods += [function(weapons,'void ApplyPhysicsBulletImpulse'),
                function(combat,'void CBaseMonster :: Killed')]
    template=(ROOT/'game_shared/tests/fixtures/j10_6_3/physics_adapter.cpp.in').read_text()
    with tempfile.TemporaryDirectory(prefix='j1063_physics_') as tmp:
        tmp=Path(tmp);unit=tmp/'physics.cpp';binary=tmp/'physics'
        unit.write_text(template.replace('@PRODUCTION@','\n\n'.join(methods)))
        args=[os.environ.get('CXX','c++'),'-std=c++17','-O0','-g',
              '-I'+str(ROOT/'game_shared'),str(unit),'-o',str(binary)]
        if os.environ.get('J1063_UBSAN'):
            args[1:1]=['-fsanitize=undefined','-fno-sanitize-recover=all']
        for command in [args,[str(binary)]]:
            result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
            if result.returncode: raise RuntimeError((result.stdout+result.stderr)[-10000:])
            if result.stdout.strip():print(result.stdout.strip())

if __name__=='__main__': main()
