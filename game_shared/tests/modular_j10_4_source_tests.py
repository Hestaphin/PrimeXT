"""J.10.4 client pose and modular registry lifetime wiring regressions."""
from pathlib import Path
base = Path(__file__).resolve().parents[2]
registry = (base/'client/weapon_predicting_context.cpp').read_text()
client = (base/'client/cdll_int.cpp').read_text()
renderer = (base/'client/render/gl_studio_draw.cpp').read_text()
header = (base/'client/render/gl_studio.h').read_text()
init = (base/'client/render/gl_studio_init.cpp').read_text()
assert 'void ResetModularClientDefinitions()' in registry
assert 'ClearModularAmmoMappings();' in registry
assert 'CBaseWeaponContext::ItemInfoArray[weapon.runtimeId] = ItemInfo{}' in registry
assert 'ResetClientWeaponPredictionForModularReload();\n        ResetModularClientDefinitions();' in registry
assert 'g_modularMappedSlots[slot] = true;' in registry
assert client.count('ResetModularClientDefinitions();') >= 3
assert 'void ResetClientWeaponPredictionForModularReload()' in client
assert 'g_pWeaponPredicting.reset();' in client
assert 'StudioEvaluatePredictionBones' in renderer and 'm_predictionCollisionBones' in renderer
assert 'CollisionPoseStateEqual(inst.m_predictionCollisionStamp.state,state)' in renderer
assert 'inst.m_predictionCollisionStamp.sampleTime==now' in renderer
physical = (base/'game_shared/studio_physical_pose.h').read_text()
assert 'inst.m_partPoseTargetPos[i]' in physical
assert 'inst.m_ragdollTargetRootOrigin' in physical
assert 'inst.m_bExternalBones' in renderer
assert 'm_predictionCollisionStamp' in header and 'm_predictionCollisionStamp' in init
assert 'StudioGetAnim(m_model, seq)' in renderer
# Prevent putting the prediction fix back into a permissive age threshold.
assert 'const float maxAge = current.timeDependent ? 0.001f' in (
    base/'game_shared/physics_collision_pose_freshness.h').read_text()
print('J.10.4 client source integration assertions: PASS')
