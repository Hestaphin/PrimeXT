from pathlib import Path
root = Path(__file__).resolve().parents[2]
src = (root/'server/physx/physx_impl.cpp').read_text()
header = (root/'game_shared/physics_asset_trace_owner.h').read_text()
fn = src[src.index('void CPhysicPhysX :: SweepTest('):src.index('void CPhysicPhysX :: SweepEntity(')]
assert '#include "physics_asset_trace_owner.h"' in src
assert 'rootHasShapes && CheckCollision(pRigidActor)' in fn
assert 'SelectMovementSweepOwners(' in fn
assert 'if( !sweepOwners.Any() )' in fn
assert 'if( !sweepOwners.root )' in fn
assert fn.index('if( !sweepOwners.root )') < fn.index('ShapeTypeToGeomType')
assert 'rootHasShapes && rootCollisionEnabled' in header
assert 'activeChildShapes' in header
assert 'pRigidActor->getNbShapes() <= 0 ||' not in fn
print('I.10.10 server root / hinged child sweep wiring PASS')
