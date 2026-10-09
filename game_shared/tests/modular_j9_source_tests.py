"""Source contract checks for J.9 geometry integrity; not an in-game test."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
read=lambda f:(ROOT/f).read_text()
manager=read('server/modular_ballistics.cpp')
exit_query=read('server/modular_ballistic_exit.cpp')
phys=read('server/physx/physx_impl.cpp')
asset=read('server/physx/physics_asset.cpp')
assert manager.index('PreparePenetration(shot,tr,body') < manager.index('Impact(shot,tr,body') < manager.index('BeginPenetration(shot,contact')
assert 'shot.penetrations >= kMaxPenetrations' in manager
assert 'FindModularBallisticExit' in manager
assert 'ValidateWorldBallisticGeometry' in manager
assert 'SameWorldPenetrationMaterial' in manager
assert 'TraceBallisticPhysicsExit(target,ragdollBodyIndex' in exit_query
assert 'reverse.pHit == target->edict()' in exit_query
assert 'entry.iHitgroup == reverse.iHitgroup' in exit_query
assert 'ValidBallisticExit' in exit_query
assert 'UTIL_TraceLine' in exit_query and 'ignore_monsters,target->edict()' in exit_query
assert 'asset->RaycastBodyExit(bodyIndex' in phys
assert 'const PhysicsPartShape &section' in phys and 'picked.shape->getGeometry()' in phys
assert 'ValidBallisticEntrySelection' in phys and 'exit=Vector(back.position.x' in phys
assert 'getNbShapes()' in phys and 'ActorFromEntity(target)' in phys
assert 'bodyIndex >= static_cast<int>(m_bodies.size())' in asset
assert 'm_bodies[bodyIndex]' in asset
assert '"modular_ballistic_exit.cpp"' in read('server/CMakeLists.txt')
assert 'TraceBallisticPhysicsExit' in read('server/world.cpp')
assert '_cp.mdl' in read('documentation/docs/eng/physics_assets.md')
print('PASS J.9: entry before damage, same-body/shape exits, studio-group check, world occlusion, null physics')
