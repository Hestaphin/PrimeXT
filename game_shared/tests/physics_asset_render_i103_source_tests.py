#!/usr/bin/env python3
from pathlib import Path
root = Path(__file__).resolve().parents[2]
render = (root/'client/render/gl_studio_draw.cpp').read_text()
render += (root/'game_shared/studio_physical_pose.h').read_text()
for usage in (
    '#include "physics_asset_render_coordinates.h"',
    'StudioPhysicalRenderScale(root)',
    'StudioScaledWorldBone(pos,q,renderScale)',
    'StudioUnitWorldBone(animated[i]).GetQuaternion()',
    'StudioAnimatedWorldRelative(animated[parent],animated[i])',
    'StudioRebuildPhysicalChild(worldBones[parent],local,renderScale)',
    'StudioScaledWorldBone(rootPos,rootQ,renderScale)',
    'StudioScaledWorldBone(bodyWorld.GetOrigin(),bodyWorld.GetQuaternion(),renderScale)',
    'StudioRebuildPhysicalChild(worldBones[child],local.Invert(),renderScale)',
):
    assert usage in render, f'missing scale-aware render path: {usage}'
assert 'm_pModelInstance->m_pbones[i]=matrix3x4(pos,q);' not in render
print('I.10.3 render scale source wiring PASS')
