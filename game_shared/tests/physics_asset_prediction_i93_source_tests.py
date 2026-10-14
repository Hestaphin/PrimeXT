#!/usr/bin/env python3
"""SDK-free source integration checks for B2/B3, not an in-game physics test."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
cl = (root/'client/utils.cpp').read_text()
renderer = (root/'client/render/gl_studio_draw.cpp').read_text()
sv = (root/'server/physx/physx_impl.cpp').read_text()
cm = (root/'game_shared/meshdesc.cpp').read_text()
fm = (root/'game_shared/meshdesc_factory.cpp').read_text()
fmt = (root/'game_shared/physics_asset_format.cpp').read_text()
assert 'PredictionPhysicsAsset(pTouch->model)' in cl
assert 'physicsasset::Parse(std::string_view' in cl
assert 'PredictionCollisionActive(section,model,body)' in cl
assert 'R_StudioGetCollisionBonePose(ent,index,boneMatrix)' in cl
assert 'pTouch->curstate.solid == SOLID_NOT' in cl
assert 'asset->collisionFingerprint' in cl
init = (root/'client/render/gl_studio_init.cpp').read_text()
assert 'StudioEnsureCpuInstance(const_cast<cl_entity_t*>(e))' in renderer
assert 'm_pEntity != entity' in init and 'inst.m_pModel != model' in init
assert 'hasPhysicsAssetCollision ? GetPhysicsAssetDefinition(pTouch)->collisionFingerprint : 0' in sv
assert 'entry.sourceFingerprint == m_sourceFingerprint' in cm
assert 'm_sourceFingerprint != fingerprint' in cm
assert 'meshDesc.Matches(mod->name, body, skin, geomType, fingerprint)' in fm
assert 'candidate.collisionFingerprint=hash?hash:1' in fmt and 'out=std::move(candidate)' in fmt
assert 'physics_asset_format.cpp' in (root/'client/CMakeLists.txt').read_text()
assert 'kFormatVersion = 3' in (root/'game_shared/clipfile.h').read_text()
print('I.9.3 client prediction and cache wiring: PASS')
