#!/usr/bin/env python3
"""Source wiring for J.10.2; not a PhysX runtime replacement."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
ballistic=(root/'server/modular_ballistics.cpp').read_text()
physx=(root/'server/physx/physx_impl.cpp').read_text()
exits=(root/'server/modular_ballistic_exit.cpp').read_text()
assert 'ValidateWorldBallisticGeometry' in ballistic
assert 'UTIL_PointContents' in ballistic
assert 'SameWorldPenetrationMaterial' in ballistic
assert 'COM_MatDefFromSurface(interval.entry_surface,entryPoint)' in ballistic
assert 'COM_MatDefFromSurface(interval.exit_surface,exitPoint)' in ballistic
assert 'TraceBallisticPhysicsExit(target,-1' in exits
block=physx.split('bool CPhysicPhysX::TraceBallisticPhysicsExit(')[1].split('// Milestone I.5')[0]
assert 'ValidBallisticEntrySelection' in block
assert block.index('ValidBallisticEntrySelection') < block.index('picked.shape->getGeometry()')
assert '!HasPhysicsAssetCollision(target)' not in block
assert '!it->second.selectionHasGeometry' not in block
assert 'state.renderBody=pEntity->pev->body;' in physx
assert 'includeRoot && pRigidActor->getNbShapes() > 0' in physx
assert 'if( !pActor )\n\t\treturn;' in physx
print('PASS J.10.2: BSP continuity, exact PhysX shape, primitive and zero-root wiring')
