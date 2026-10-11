"""J.8 source-contract regression checks; not a substitute for running Xash."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
def read(name): return (ROOT / name).read_text()

def main():
    combat = read('server/modular_ballistics.cpp')
    server_frame = read('server/client.cpp')
    build = read('server/CMakeLists.txt')
    physics = read('server/physx/physx_impl.cpp')
    cvars = read('server/game.cpp')
    assert 'class CModularBallisticManager' in combat
    assert 'LINK_ENTITY_TO_CLASS(modular_ballistic_manager' in combat
    assert 'LINK_ENTITY_TO_CLASS(modular_ballistic_projectile' not in combat
    assert 'modular_ballistic_projectile"' not in combat
    assert 'CModularBullet' not in combat
    assert 'SetThink(' not in combat
    assert 'UpdateModularBallisticManager();' in server_frame
    assert 'WorldPhysic->Update( gpGlobals->frametime );' in server_frame
    assert 'PrecomputeBallisticCurve' in combat
    assert 'SampleBallisticCurve' in combat
    assert 'ComputePenetration' in combat
    assert 'ValidateWorldBallisticGeometry' in combat
    assert 'SameWorldPenetrationMaterial' in combat
    assert 'shot.penetrations >= kMaxPenetrations' in combat
    assert 'DEFINE_ARRAY(m_savedOwners,FIELD_EHANDLE' in combat
    assert 'DEFINE_ARRAY(m_savedRecords,FIELD_CHARACTER' in combat
    assert 'FTRACE_MATERIAL_TRACE | FTRACE_BULLET_PHYSICS' in combat
    assert 'ResolveRagdollBulletCollision' in combat
    assert 'ResolveBulletImpactEffects' in combat
    assert 'ApplyPhysicsBulletImpulse' in combat
    assert 'gpGlobals->trace_flags = previousTraceFlags' in combat
    assert 'ClearMultiDamage(); // delayed projectile' in combat
    assert 'sv_ballistics_penetration' in cvars
    assert 'ballistic_path_cache.cpp' in build
    assert 'ballistic_penetration.cpp' in build
    assert 'SelectMovementSweepOwners' in physics
    assert 'physics_asset_trace_owner.h' in physics
    assert 'pxmv' in read('tools/weapon_editor/CMakeLists.txt')
    print('PASS J.8: central frame manager, cached flight, conservative material penetration, I.10.10 collision and save compatibility hooks')
if __name__ == '__main__': main()
