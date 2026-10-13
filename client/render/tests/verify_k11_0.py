#!/usr/bin/env python3
"""K.11.0 cache corruption, actual subview allocator faults and full ragdoll CCD regressions.
GPU/engine/filesystem/PhysX services are adapters; this is not a game or solver run.
"""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from verify_map_transition import function


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    root = args.source_root.resolve()
    tests = root / 'client/render/tests'
    temporary = tempfile.TemporaryDirectory(prefix='primext-k11-0-') if not args.work_dir else None
    work = args.work_dir.resolve() if args.work_dir else Path(temporary.name)
    work.mkdir(parents=True, exist_ok=True)
    if os.name == 'posix':
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ, TMPDIR=str(work))
    flags = ['-std=c++17', '-O1', '-g', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie',
             '-fsanitize=address,undefined', '-ffunction-sections', '-fdata-sections',
             '-Wno-write-strings', '-DCLIENT_DLL=1', '-DCLIENT_WEAPONS=1',
             '-D_LINUX=1', '-DLINUX=1', '-D_vsnprintf=vsnprintf']
    includes = ['-I' + str(root / p) for p in ['client', 'client/render', 'common', 'engine', 'game_shared', 'public']]
    results = []

    def run(name, command):
        result = subprocess.run([str(c) for c in command], cwd=root, env=env, capture_output=True, text=True)
        (work / (name + '.log')).write_text(result.stdout + result.stderr)
        results.append({'test': name, 'returncode': result.returncode, 'command': [str(c) for c in command]})
        print(name, 'PASS' if result.returncode == 0 else 'FAIL', flush=True)
        if result.returncode:
            print((result.stdout + result.stderr)[-3500:], flush=True)
        return result.returncode == 0

    cache = (tests / 'cache_j9_10_harness.cpp.in').read_text().replace('/* BRUSH_NAMES */', '"*1","*2"')
    (work / 'cache.cpp').write_text(cache)
    command = [os.environ.get('CXX', 'c++'), *flags, *includes, work / 'cache.cpp',
               *[root / p for p in ['game_shared/meshdesc.cpp', 'game_shared/virtualfs.cpp',
                 'game_shared/stringlib.cpp', 'game_shared/matrix.cpp', 'game_shared/mathlib.cpp']],
               '-Wl,--gc-sections', '-o', work / 'cache']
    if run('cache_compile', command):
        cases = ['brush_load', 'brush_path', 'brush_private_load', 'null_cache', 'studio_paths',
                 'studio_extensionless', 'studio_missing', 'studio_roundtrip', 'studio_append',
                 'bad_facet_count', 'bad_first_index', 'bad_facet_zero', 'bad_facet_range',
                 'bad_unused_indexes', 'bad_vertex_nan', 'bad_bounds', 'bad_plane_index',
                 'bad_plane_nan', 'bad_plane_zero', 'bad_lump_offset', 'bad_lump_length',
                 'bad_lump_entry', 'bad_lump_overlap', 'bad_lump_size', 'bad_entry_offset',
                 'bad_entry_length', 'bad_entry_count', 'bad_table_offset', 'bad_short_header',
                 'bad_short_table', 'bad_short_data']
        for case in cases:
            run(case, [work / 'cache', case])

    # Reuse the existing view/world adapters and their production function selection.
    parsed = ast.parse((tests / 'verify_j10_6_9_10.py').read_text())
    selection = next(ast.literal_eval(n.value) for n in ast.walk(parsed)
                     if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'selection' for t in n.targets))
    chunks = [function((root / path).read_text(), name) for path, names in selection.items() for name in names]
    chunks.append(function((root / 'client/render/gl_world_new.cpp').read_text(), 'Mod_SetWorldSurfaceTextureFlags'))
    shader = (root / 'client/render/gl_shader.cpp').read_text()
    for name in ['SetShader', 'IsValid']:
        chunks.append(function(shader.replace('shader_t :: ' + name, 'shader_t_' + name),
                               'shader_t_' + name).replace('shader_t_' + name, 'shader_t::' + name))
    adapter = (tests / 'subview_j9_10_harness.cpp.in').read_text()
    adapter = adapter.replace(function(adapter, 'R_AllocateSubviewTexture'), '')
    adapter = adapter.replace(function(adapter, 'GL_BindFBO'), '')
    driver = (tests / 'subview_alloc_k11_0_adapters.cpp.in').read_text()
    backend = (root / 'client/render/gl_backend.cpp').read_text()
    allocator = '\n'.join(function(backend, name) for name in ['R_AllocFrameBuffer', 'R_FreeFrameBuffer', 'GL_BindFrameBuffer', 'GL_BindFBO'])
    allocator += '\n' + function((root / 'client/render/gl_subview.cpp').read_text(), 'R_AllocateSubviewTexture')
    adapter = adapter.replace('std::string test = argv[1];', '''std::string test = argv[1]; SetupDriverApi();
    if(test == "backend_texture_failure" || test == "fallback_texture_failure") driverFault = 1;
    if(test == "backend_gl_texture_failure") driverFault = 2;
    if(test == "backend_depth_failure") driverFault = 3;
    if(test == "backend_framebuffer_failure") driverFault = 4;
    if(test == "backend_incomplete") driverFault = 5;
    if(test == "backend_capacity") tr.num_framebuffers = MAX_FRAMEBUFFERS;
    if(test.find("fallback_") == 0) fbo = false;
    glState.frameBuffer = driverFramebuffer = 77;''')
    needle = 'if(test.find("camera_") != std::string::npos || test == "parent_stale") {'
    adapter = adapter.replace(needle, '''if(test.find("backend_") == 0 || test == "fallback_texture_failure") {
            const bool capacity = test == "backend_capacity";
            assert(capturedCalls == (fbo ? 0 : 1) && extra.subtexture[0] == 0);
            assert(tr.num_subview_used == 0 && tr.num_framebuffers == (capacity ? MAX_FRAMEBUFFERS : 0));
            assert(glState.frameBuffer == 77 && driverFramebuffer == 77 && copyCalls == 0);
            assert(freedTextures.size() == (driverFault == 1 ? 0u : 1u));
            assert(deletedRenderbuffers == (driverFault == 4 || driverFault == 5 ? 1 : 0));
            assert(deletedFramebuffers == (driverFault == 5 ? 1 : 0));
            // A failed attempt must not poison allocation or consume the pool.
            driverFault = 0; tr.num_framebuffers = 0;
            assert(R_AllocateSubviewTexture(CViewport(0, 0, 256, 256), TF_NOMIPMAP) == 1);
            assert(tr.num_subview_used == 1 && tr.subviewTextures[0].texturenum.Initialized());
        } else ''' + needle)
    adapter = adapter.replace('} else { assert(false && "Unknown case"); }', '''} else if(test == "allocation_reuse") {
                const CViewport port(capturedView.viewport);
                tr.realframecount++;
                const int before = createdTextures;
                assert(R_AllocateSubviewTexture(port, tr.subviewTextures[0].texturenum.GetFlags()) == 1);
                assert(tr.num_subview_used == 1 && tr.num_framebuffers == 1 && createdTextures == before);
            } else if(test == "fallback_valid") { assert(copyCalls == 1); }
            else { assert(false && "Unknown case"); }''')
    template = (tests / 'map_transition_harness.cpp.in').read_text().split('int main(int argc, char **argv) {')[0]
    template = template.replace('/* ASAN_DEFAULT_OPTIONS */', 'detect_leaks=0:halt_on_error=1')
    template = template.replace('bool GL_Support(int) { return false; }',
                               'static bool npot=true, fbo=true;\nbool GL_Support(int cap) { return cap==R_ARB_TEXTURE_NPOT_EXT ? npot : cap==R_FRAMEBUFFER_OBJECT ? fbo : false; }')
    pre, main_case = adapter.split('/* SUBVIEW_MAIN */')
    source = template.replace('/* PRODUCTION_FUNCTIONS */', pre + '\n' + driver + '\n' + allocator + '\n' + '\n'.join(chunks)) + '\n' + main_case
    origins = {'PORTAL_ORIGIN':'-103.4,0.6,-40', 'DESTINATION_ORIGIN':'-106,890,-872',
               'DESTINATION_ANGLES':'28.8,61.5,30', 'MONITOR_ORIGIN':'-65,1083,-925',
               'CAMERA_ORIGIN':'-188,764,-880', 'CAMERA_ANGLES':'4,64.1,0',
               'PORTAL_TEXTURE_NAMES':'"PORTAL","MONITOR","REFLECT","SKY"'}
    for marker, value in origins.items():
        source = source.replace('/* ' + marker + ' */', value)
    (work / 'subview.cpp').write_text(source)
    command = [os.environ.get('CXX', 'c++'), *flags, *includes, work / 'subview.cpp',
               *[root / p for p in ['client/render/texture_handle.cpp', 'game_shared/stringlib.cpp',
                 'game_shared/matrix.cpp', 'game_shared/mathlib.cpp', 'client/render/gl_viewport.cpp',
                 'game_shared/frustum.cpp']], '-Wl,--gc-sections', '-o', work / 'subview']
    if run('subview_compile', command):
        cases = ['bsp_portal', 'bsp_monitor', 'portal_valid', 'screen_remote', 'screen_local',
                 'screen_local_thirdperson', 'portal_camera_missing', 'portal_camera_stale',
                 'screen_camera_missing', 'screen_camera_stale', 'parent_stale', 'portal_clone_same',
                 'portal_clone_distinct', 'backend_texture_failure', 'backend_gl_texture_failure',
                 'backend_depth_failure', 'backend_framebuffer_failure', 'backend_incomplete',
                 'backend_capacity', 'fallback_texture_failure', 'fallback_valid', 'allocation_reuse']
        for case in cases:
            run(case, [work / 'subview', case])
    run('full_ragdoll_ccd', [sys.executable, root / 'game_shared/tests/ragdoll_f3_physx_integration.py', '--ubsan'])
    sources = ['game_shared/meshdesc.cpp', 'server/physx/physics_asset.cpp',
               'client/render/gl_subview.cpp', 'client/render/gl_backend.cpp']
    summary = {'milestone': 'K.11.0', 'passed': sum(r['returncode'] == 0 for r in results),
               'total': len(results), 'results': results,
               'source_sha256': {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in sources},
               'scope': 'ASan/UBSan production cache and subview methods; recording PhysX/GL/filesystem/engine adapters; no native SDK, solver or GPU run.'}
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(summary, indent=2) + '\n')
    print(f"K.11.0 regressions: {summary['passed']}/{summary['total']} PASS", flush=True)
    return int(summary['passed'] != summary['total'])


if __name__ == '__main__':
    raise SystemExit(main())
