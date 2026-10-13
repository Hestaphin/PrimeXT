from pathlib import Path
r=Path(__file__).resolve().parents[2]
sv=(r/'server/physx/physx_impl.cpp').read_text()
cl=(r/'client/utils.cpp').read_text()
shared=(r/'game_shared/physics_asset_primitive_trace.h').read_text()
assert 'PrimitiveHingeBodyIndex' in shared
assert 'ShapeType::Box' in shared and 'ShapeType::Sphere' in shared and 'ShapeType::Capsule' in shared
assert 'shape.collisionIndex == asset.collisions.size()' in sv
assert 'primitive.part->actor->getGlobalPose()' in sv
assert 'physicsasset::BuildPrimitiveTraceTriangles' in sv and 'physicsasset::BuildPrimitiveTraceTriangles' in cl
assert 'PredictionHasPrimitiveHinges' in cl
assert 'R_StudioGetCollisionBonePose(ent,primitive.boneIndex' in cl
assert 'PrimitiveHingeBodyIndex(asset,part)' in cl
assert 'const PxBounds3 bounds = part.actor->getWorldBounds()' in sv
assert 'const bool keepLegacyRoot' in sv and 'havePrimitiveOnlyResult' in sv
assert 'havePrimitiveOnlyResult' in cl and 'mergePrimitiveOnly();' in cl
print('I.10.4 movement, prediction, active primitive and bounds wiring PASS')
