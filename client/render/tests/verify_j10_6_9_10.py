#!/usr/bin/env python3
"""Production brush-cache and portal/monitor checks; no running game or GPU."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_map_transition import function
from verify_bsp_render import bsp, legacy_client_flags


def run(command, work, name):
    result = subprocess.run(command, capture_output=True, text=True,
                            env=dict(os.environ, TMPDIR=str(work)))
    (work / (name + '.log')).write_text(result.stdout + result.stderr)
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    p.add_argument('--physbox-map', type=Path, required=True)
    p.add_argument('--portal-map', type=Path, required=True)
    p.add_argument('--work-dir', type=Path, required=True)
    p.add_argument('--json', type=Path, required=True)
    p.add_argument('--expect-legacy-failures', action='store_true')
    a = p.parse_args()
    if os.name == 'posix':
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    root = a.source_root.resolve()
    work = a.work_dir.resolve(); work.mkdir(parents=True, exist_ok=True)
    tests = Path(__file__).resolve().parent
    maps = [bsp(a.physbox_map), bsp(a.portal_map)]
    assert maps[0]['entity_classes'].count('func_physbox') == 4
    assert {'PORTAL', 'MONITOR', 'REFLECT', 'SKY'}.issubset(maps[1]['texture_names'])
    import struct
    def entities(path):
        data = path.read_bytes(); off, length = struct.unpack_from('<ii', data, 4)
        text = data[off:off + length].decode('latin1').rstrip('\0')
        return [dict(re.findall(r'"([^"\n]*)"\s*"([^"\n]*)"', s))
                for s in re.findall(r'\{([^}]*)\}', text)]
    boxes = entities(a.physbox_map)
    brush_names = [e['model'] for e in boxes if e.get('classname') == 'func_physbox']
    portal_entities = entities(a.portal_map)
    portal = next(e for e in portal_entities if e.get('classname') == 'func_portal')
    monitor = next(e for e in portal_entities if e.get('classname') == 'func_monitor')
    destination = next(e for e in portal_entities if e.get('targetname') == portal['target'])
    camera = next(e for e in portal_entities if e.get('targetname') == monitor['camera'])
    flags = ['-std=c++17', '-O1', '-g', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie',
             '-fsanitize=address,undefined', '-ffunction-sections', '-fdata-sections',
             '-Wno-write-strings', '-DCLIENT_DLL=1', '-DCLIENT_WEAPONS=1',
             '-D_LINUX=1', '-DLINUX=1', '-D_vsnprintf=vsnprintf']
    includes = ['-I' + str(root / f) for f in
                ['client', 'client/render', 'common', 'engine', 'game_shared', 'public']]
    commands = []
    cache = (tests / 'cache_j9_10_harness.cpp.in').read_text().replace(
        '/* BRUSH_NAMES */', ','.join(json.dumps(n) for n in brush_names))
    (work / 'cache.cpp').write_text(cache)
    commands.append(['g++', *flags, *includes, str(work / 'cache.cpp'),
        *[str(root / f) for f in ['game_shared/meshdesc.cpp', 'game_shared/virtualfs.cpp',
          'game_shared/stringlib.cpp', 'game_shared/matrix.cpp', 'game_shared/mathlib.cpp']],
        '-Wl,--gc-sections', '-o', str(work / 'cache')])
    compiled = run(commands[-1], work, 'cache_compile')
    if compiled.returncode:
        print(compiled.stderr[-9000:]); raise SystemExit(compiled.returncode)
    template = (tests / 'map_transition_harness.cpp.in').read_text()
    template = template.split('int main(int argc, char **argv) {')[0]
    template = template.replace('/* ASAN_DEFAULT_OPTIONS */', 'detect_leaks=0:halt_on_error=1')
    template = template.replace('bool GL_Support(int) { return false; }',
        'static bool npot=true, fbo=true;\nbool GL_Support(int cap) { return cap==R_ARB_TEXTURE_NPOT_EXT ? npot : cap==R_FRAMEBUFFER_OBJECT ? fbo : false; }')
    world = (root / 'client/render/gl_world_new.cpp').read_text()
    subview = (root / 'client/render/gl_subview.cpp').read_text()
    selection = {
        'client/utils.cpp': ['UTIL_IsLocal'],
        'client/render/gl_world_new.cpp': ['Mod_ClearRenderContext', 'Mod_CopyMaterialDesc',
            'Mod_LoadWorldMaterials', 'Surf_CheckSubview', 'Mod_ShaderSceneForward', 'Mod_ShaderLightForward',
            'Mod_PrecacheShaders', 'Mod_DeleteBufferObject', 'Mod_ThrowModelInstances', 'Mod_FreeWorld', 'R_ProcessWorldData'],
        'client/render/gl_rsurf.cpp': ['R_TextureAnimation'],
        'client/render/gl_rmain.cpp': ['R_InitRefState', 'R_ClearFrameLists', 'R_ResetRefState',
            'R_PushRefState', 'R_PopRefState', 'R_GetPrevInstance', 'HUD_BuildLightmaps'],
        'client/render/gl_decals.cpp': ['ClearDecals'],
        'client/render/gl_subview.cpp': ['R_SetupMirrorView', 'R_SetupPortalView', 'R_SetupScreenView',
            'R_CheckMirrorClone', 'R_CheckScreenSource', 'R_CheckPortalSource', 'R_CanSkipPass',
            'R_CalcMirrorSubviewMatrix', 'R_CalcPortalSubviewMatrix', 'R_CalcSubviewMatrix',
            'R_GetSubviewCamera', 'R_RenderSubview']}
    chunks = [function((root / path).read_text(), n, n in {'Mod_ClearRenderContext','R_GetSubviewCamera'})
              for path, names in selection.items() for n in names]
    chunks.append(function(world, 'Mod_SetWorldSurfaceTextureFlags', True) or legacy_client_flags(world))
    shader = (root / 'client/render/gl_shader.cpp').read_text()
    for name in ['SetShader', 'IsValid']:
        normalized = shader.replace('shader_t :: ' + name, 'shader_t_' + name)
        chunks.append(function(normalized, 'shader_t_' + name).replace('shader_t_' + name, 'shader_t::' + name))
    adapter = (tests / 'subview_j9_10_harness.cpp.in').read_text()
    pre, main = adapter.split('/* SUBVIEW_MAIN */')
    template = template.replace('/* PRODUCTION_FUNCTIONS */', pre + '\n' + '\n'.join(chunks))
    cpp = template + '\n' + main
    for marker, entity, key in [('PORTAL_ORIGIN',portal,'origin'), ('DESTINATION_ORIGIN',destination,'origin'),
          ('DESTINATION_ANGLES',destination,'angles'), ('MONITOR_ORIGIN',monitor,'origin'),
          ('CAMERA_ORIGIN',camera,'origin'), ('CAMERA_ANGLES',camera,'angles')]:
        cpp = cpp.replace('/* '+marker+' */', ','.join(entity[key].split()))
    cpp = cpp.replace('/* PORTAL_TEXTURE_NAMES */', ','.join(json.dumps(n) for n in maps[1]['texture_names']))
    (work / 'subview.cpp').write_text(cpp)
    commands.append(['g++', *flags, *includes, str(work / 'subview.cpp'),
        *[str(root / f) for f in ['client/render/texture_handle.cpp', 'game_shared/stringlib.cpp',
          'game_shared/matrix.cpp', 'game_shared/mathlib.cpp', 'client/render/gl_viewport.cpp',
          'game_shared/frustum.cpp']], '-Wl,--gc-sections', '-o', str(work / 'subview')])
    compiled = run(commands[-1], work, 'subview_compile')
    if compiled.returncode:
        print(compiled.stderr[-9000:]); raise SystemExit(compiled.returncode)
    cases = {
        'cache': ['brush_load','brush_path','brush_private_load','null_cache','studio_paths',
                  'studio_extensionless','studio_missing','studio_roundtrip'],
        'subview': ['bsp_portal','bsp_monitor','portal_valid','screen_remote','screen_local',
            'screen_local_thirdperson','portal_camera_missing','portal_camera_stale',
            'screen_camera_missing','screen_camera_stale','parent_stale','portal_clone_same',
            'portal_clone_distinct','fallback_allocation_failure','fbo_allocation_failure']}
    results = {}
    for binary, names in cases.items():
        for name in names:
            result = run([str(work / binary), name], work, name)
            results[name] = {'passed':result.returncode == 0,'returncode':result.returncode}
            print(name + ': ' + ('PASS' if result.returncode == 0 else 'FAIL'), flush=True)
    reproduced = {'brush_load','brush_path','brush_private_load','null_cache','studio_extensionless','studio_roundtrip',
        'screen_remote','screen_local','screen_local_thirdperson','portal_camera_missing',
        'portal_camera_stale','screen_camera_missing','screen_camera_stale',
        'parent_stale','portal_clone_distinct','fallback_allocation_failure','fbo_allocation_failure'}
    controls = {'studio_paths','studio_missing','bsp_portal','bsp_monitor',
        'portal_clone_same'}
    accepted = all(v['passed'] for v in results.values()) if not a.expect_legacy_failures else (
        all(not results[n]['passed'] for n in reproduced) and all(results[n]['passed'] for n in controls))
    a.json.parent.mkdir(parents=True, exist_ok=True)
    source_paths = ['game_shared/meshdesc.cpp','client/render/gl_subview.cpp','client/render/gl_world_new.cpp']
    report = {'accepted':accepted,'cases':results,'source_root':str(root),
        'source_sha256':{f:hashlib.sha256((root/f).read_bytes()).hexdigest() for f in source_paths},
        'maps':maps,'brush_model_names':brush_names,'portal_entities':portal_entities,
        'compile_commands':commands,'legacy_expected_failures':sorted(reproduced) if a.expect_legacy_failures else [],
        'scope':'Full production meshdesc.cpp and production subview functions; actual BSP names and entity transforms. Filesystem/engine/GL/view adapters; no PhysX SDK, gameplay or GPU run.'}
    a.json.write_text(json.dumps(report,indent=2)+'\n')
    if not accepted:
        for name,result in results.items():
            if not result['passed'] and not a.expect_legacy_failures:
                print((work/(name+'.log')).read_text()[-3000:])
    raise SystemExit(0 if accepted else 1)


if __name__ == '__main__':main()
