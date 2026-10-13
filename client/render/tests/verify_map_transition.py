#!/usr/bin/env python3
"""Exercise production map-lifetime functions with real headers and API adapters.

This is not an engine/GPU test. BSP parsing, the driver and the studio-instance
owner are adapters; shader selection, texture animation, render-context reset,
material creation and world cleanup are extracted unchanged from source.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name, optional=False):
    masked = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    lambda m: ' ' * len(m.group()), source)
    match = re.search(r'^(?:static )?[\w *]+\b' + re.escape(name) + r'\s*\([^;{}]*\)\s*\{',
                      masked, re.M)
    if not match:
        if optional:
            return ''
        raise RuntimeError('Missing production function: ' + name)
    start = masked.index('{', match.start())
    depth = 1
    end = start + 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return source[match.start():end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path,
                        default=Path(__file__).resolve().parents[3])
    parser.add_argument('--json', type=Path)
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--expect-legacy-failures', action='store_true')
    parser.add_argument('--leak-sanitizer', action='store_true',
                        help='Enable LSan where /proc and ptrace support are available.')
    args = parser.parse_args()
    root = args.source_root.resolve()
    fixture = Path(__file__).with_name('map_transition_harness.cpp.in').read_text()
    selection = {
        'client/render/gl_world_new.cpp': [
            'Mod_ClearRenderContext', 'Mod_CopyMaterialDesc', 'Mod_LoadWorldMaterials',
            'Surf_CheckSubview', 'Mod_ShaderSceneForward', 'Mod_ShaderLightForward',
            'Mod_PrecacheShaders', 'Mod_DeleteBufferObject', 'Mod_ThrowModelInstances',
            'Mod_FreeWorld', 'R_ProcessWorldData'],
        'client/render/gl_rsurf.cpp': ['R_TextureAnimation'],
        'client/render/gl_rmain.cpp': ['R_InitRefState', 'HUD_BuildLightmaps'],
        'client/render/gl_decals.cpp': ['ClearDecals'],
        'client/render/gl_shader.cpp': []}
    chunks = []
    hashes = {}
    for path, names in selection.items():
        data = (root / path).read_text()
        hashes[path] = hashlib.sha256((root / path).read_bytes()).hexdigest()
        for name in names:
            body = function(data, name, name == 'Mod_ClearRenderContext')
            if body:
                chunks.append(body)
    # shader_t methods use qualified names and need only these two definitions.
    data = (root / 'client/render/gl_shader.cpp').read_text()
    for name in ['SetShader', 'IsValid']:
        normalized = data.replace('shader_t :: ' + name, 'shader_t_' + name)
        chunks.append(function(normalized, 'shader_t_' + name)
                      .replace('shader_t_' + name, 'shader_t::' + name))
    # Order forward-declared functions explicitly; shader selection calls animation.
    options = 'detect_leaks=' + str(int(args.leak_sanitizer)) + ':halt_on_error=1'
    generated = fixture.replace('/* PRODUCTION_FUNCTIONS */', '\n\n'.join(chunks))
    generated = generated.replace('/* ASAN_DEFAULT_OPTIONS */', options)
    temp = tempfile.TemporaryDirectory(prefix='primext-map-transition-') if not args.work_dir else None
    work = args.work_dir.resolve() if args.work_dir else Path(temp.name)
    work.mkdir(parents=True, exist_ok=True)
    cpp = work / 'map_transition.cpp'
    cpp.write_text(generated)
    exe = work / 'map_transition'
    flags = ['-std=c++17', '-O1', '-g', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie',
             '-fsanitize=address,undefined', '-ffunction-sections', '-fdata-sections',
             '-Wno-write-strings', '-DCLIENT_DLL=1', '-DCLIENT_WEAPONS=1',
             '-DXASH_64BIT=1', '-D_LINUX=1', '-DLINUX=1', '-D_vsnprintf=vsnprintf']
    command = ['g++', *flags, *['-I' + str(root / p) for p in
               ['client', 'client/render', 'common', 'engine', 'game_shared', 'public']],
               str(cpp), str(root / 'client/render/texture_handle.cpp'),
               str(root / 'game_shared/stringlib.cpp'), str(root / 'game_shared/matrix.cpp'),
               str(root / 'client/render/gl_viewport.cpp'), '-Wl,--gc-sections', '-o', str(exe)]
    compiled = subprocess.run(command, capture_output=True, text=True)
    (work / 'compile.log').write_text(compiled.stdout + compiled.stderr)
    if compiled.returncode:
        print(compiled.stdout + compiled.stderr)
        raise SystemExit(compiled.returncode)
    cases = ['stale_world_load', 'null_world_load', 'disconnect', 'recursive_context',
             'runtime_animation', 'parent_shader', 'repeated_unload',
             'material_lifetime', 'missing_texture_cleanup',
             'disconnected_decals', 'disconnected_lightmaps']
    results = {}
    expected_failures = set(cases) - {'runtime_animation', 'parent_shader'}
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + str(int(args.leak_sanitizer)) + ':halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    accepted = True
    for case in cases:
        run = subprocess.run([str(exe), case], capture_output=True, text=True, env=env)
        log = run.stdout + run.stderr
        (work / (case + '.log')).write_text(log)
        passed = run.returncode == 0
        expected = not (args.expect_legacy_failures and case in expected_failures)
        accepted &= passed == expected
        results[case] = {'passed': passed, 'returncode': run.returncode,
                         'expected_pass': expected, 'log': str(work / (case + '.log'))}
        print(case + ': ' + ('PASS' if passed else 'FAIL') +
              (' (expected)' if not passed and not expected else ''))
    report = {'accepted': accepted, 'source_root': str(root), 'source_sha256': hashes,
              'compile_command': command, 'cases': results, 'lsan_enabled': args.leak_sanitizer,
              'scope': 'Production function harness; BSP/engine/driver adapters, not a game run.'}
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2) + '\n')
    if not accepted:
        for case, result in results.items():
            if result['passed'] != result['expected_pass']:
                print((work / (case + '.log')).read_text()[:8000])
    raise SystemExit(0 if accepted else 1)


if __name__ == '__main__':
    main()
