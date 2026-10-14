#!/usr/bin/env python3
"""K.11.4.1.1 profiling: native objects, recording adapters, sanitizers and retained regressions.

Requires a GCC-compatible C++17 compiler. This does not run a real GPU or
PhysX solver and does not link a complete client/server game build.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
from ragdoll_f0_integration import function

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--json', type=Path)
    parser.add_argument('--skip-regressions', action='store_true')
    parser.add_argument('--no-sanitizers', action='store_true')
    args = parser.parse_args()
    temp = tempfile.TemporaryDirectory(prefix='primext-perf-k11411-') if not args.work_dir else None
    work = args.work_dir.resolve() if args.work_dir else Path(temp.name)
    work.mkdir(parents=True, exist_ok=True)
    compiler = shlex.split(os.environ.get('CXX', 'g++'))
    results = []

    def run(name, command, environment=None):
        result = subprocess.run([str(value) for value in command], cwd=ROOT,
                                env=environment, capture_output=True, text=True)
        (work / f'{name}.log').write_text(result.stdout + result.stderr)
        results.append({'name': name, 'returncode': result.returncode,
                        'command': [str(value) for value in command]})
        print(name, 'PASS' if result.returncode == 0 else 'FAIL', flush=True)
        if result.returncode:
            print((result.stdout + result.stderr)[-5000:], flush=True)
        return result.returncode == 0

    common = ['-std=c++17', '-fno-exceptions', '-Wno-write-strings']
    sanitize = [] if args.no_sanitizers else ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-pie', '-no-pie']
    strict = ['-Wall', '-Wextra', '-Werror', '-Wpedantic']
    shared = [ROOT / 'game_shared/performance_profile.cpp', ROOT / 'game_shared/performance_gpu.cpp']
    include_shared = ['-I' + str(ROOT / 'game_shared')]
    for optimization in ('O0', 'O2'):
        exe = work / ('core_' + optimization)
        if run('core_compile_' + optimization, [*compiler, *common, '-' + optimization, '-g', *strict, *sanitize,
            '-DPXPERF_TEST_CLOCK', *include_shared, ROOT / 'game_shared/tests/performance_k11411_tests.cpp',
            *shared, '-o', exe]):
            run('core_run_' + optimization, [exe])

    exe = work / 'macro'
    if run('windows_macro_compile', [*compiler, *common, '-O2', *strict, *sanitize,
        *include_shared, ROOT / 'game_shared/tests/performance_k11411_macro.cpp', *shared, '-o', exe]):
        run('windows_macro_run', [exe])

    # Step 4 deliberately replaces the unbounded 100 Hz loop. Retain the frozen
    # original comparison at legacy rate in the scheduling fixture, and test
    # bounded/rate-selected semantics instead of demanding the old overload bug.
    scheduling = ROOT / 'game_shared/tests/fixtures/scheduling_k11414/physics.cpp.in'
    fixture = (scheduling if scheduling.exists() else
        ROOT / 'game_shared/tests/fixtures/performance_k11411_physics.cpp.in').read_text()
    update = function((ROOT / 'server/physx/physx_impl.cpp').read_text(), 'void CPhysicPhysX :: Update(')
    unit = work / 'physics.cpp'
    unit.write_text(fixture.replace('@PRODUCTION@', update))
    exe = work / 'physics'
    if run('physics_method_compile', [*compiler, *common, '-O1', '-g', *strict, *sanitize,
        *include_shared, unit, shared[0], '-o', exe]):
        run('physics_method_run', [exe])

    definitions = ['-D_LINUX=1', '-DLINUX=1', '-D_vsnprintf=vsnprintf', '-D_snprintf=snprintf',
                   '-Dstricmp=strcasecmp', '-Dstrnicmp=strncasecmp']
    client_definitions = [*definitions, '-DCLIENT_DLL=1', '-DCLIENT_WEAPONS=1']
    client_include = ['-I' + str(ROOT / path) for path in
        ['client', 'client/render', 'client/ui', 'common', 'engine', 'game_shared', 'public']]
    server_include = ['-I' + str(ROOT / path) for path in
        ['server', 'server/monsters', 'server/weapons', 'server/entities', 'server/gamerules',
         'common', 'engine', 'game_shared', 'public']]
    exe = work / 'gl_adapter'
    if run('gl_adapter_compile', [*compiler, *common, '-O1', '-g', *sanitize, '-DPXPERF_TEST_CLOCK',
        '-ffunction-sections', '-fdata-sections', *client_definitions, *client_include,
        ROOT / 'game_shared/tests/performance_k11411_gl.cpp', ROOT / 'client/render/gl_performance.cpp',
        *shared, '-Wl,--gc-sections', '-o', exe]):
        run('gl_adapter_run', [exe])

    client_sources = ['client/render/gl_performance.cpp', 'client/render/gl_cvars.cpp',
        'client/render/gl_rmain.cpp', 'client/render/gl_subview.cpp', 'client/render/gl_shadows.cpp',
        'client/render/gl_backend.cpp', 'client/render/gl_rmisc.cpp', 'client/render/gl_studio_draw.cpp',
        'client/render/gl_export.cpp', 'game_shared/meshdesc_factory.cpp',
        'game_shared/performance_profile.cpp', 'game_shared/performance_gpu.cpp']
    server_sources = ['server/client.cpp', 'server/game.cpp', 'server/modular_ballistics.cpp',
        'game_shared/meshdesc_factory.cpp', 'game_shared/performance_profile.cpp']
    objects = work / 'objects'
    objects.mkdir(exist_ok=True)
    for side, sources, defines, includes in [('client', client_sources, client_definitions, client_include),
                                             ('server', server_sources, definitions, server_include)]:
        for optimization in ('O0', 'O2'):
            for source in sources:
                name = f'{side}_{optimization}_{Path(source).stem}'
                run(name, [*compiler, *common, '-' + optimization, '-Wno-invalid-offsetof', '-Wno-pointer-arith', *defines, *includes,
                    '-c', ROOT / source, '-o', objects / (name + '.o')])

    spec = importlib.util.spec_from_file_location('extract_profile_csv', ROOT / 'tools/performance/extract_profile_csv.py')
    extractor = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(extractor)
    rows = extractor.extract('''[11:20:01] PERF_K11411,client,1,frame_cpu,ms,100,5.0,4.0,8.0,9.0,10.0,100
PERF_K11411_HEADER,side,window,metric,unit,samples,mean,p50,p95,p99,max,calls
PERF_K11411,server,1,physics,ms,100,nan,1,1,1,1,100
PERF_K11411,client,2,physics,ms,0,1,1,1,1,1,0
PERF_K11411,server,2,physics_steps,count,100,1,1,1,1,1,100
''')
    extraction_ok = len(rows) == 2 and rows[0][0] == 'client' and rows[1][0] == 'server'
    results.append({'name': 'csv_extractor', 'returncode': 0 if extraction_ok else 1})
    print('csv_extractor', 'PASS' if extraction_ok else 'FAIL', flush=True)

    if not args.skip_regressions:
        # Existing tests extract production methods into service adapters. Supply
        # the actual profiling services to those fixtures; no no-op replacements.
        wrapper = work / 'profile_cxx'
        wrapper.write_text('#!' + sys.executable + '\n' +
            'import os, subprocess, sys\n' +
            'args = sys.argv[1:]\n' +
            'root = ' + repr(str(ROOT)) + '\n' +
            'args += ["-I" + root + "/game_shared", "-include", root + "/game_shared/performance_gpu.h"]\n' +
            'if not any(flag in args for flag in ["-c", "-E", "-fsyntax-only"]):\n' +
            '    for source in ["performance_profile.cpp", "performance_gpu.cpp"]:\n' +
            '        if not any(value.endswith("/" + source) for value in args): args.append(root + "/game_shared/" + source)\n' +
            'sys.exit(subprocess.call(' + repr(compiler) + ' + args))\n')
        wrapper.chmod(0o755)
        environment = dict(os.environ, CXX=str(wrapper))
        run('retained_k11_0', [sys.executable, ROOT / 'client/render/tests/verify_k11_0.py',
            '--work-dir', work / 'k11_0', '--json', work / 'k11_0.json'], environment)
        run('retained_map_transition', [sys.executable, ROOT / 'client/render/tests/verify_map_transition.py',
            '--work-dir', work / 'map_transition', '--json', work / 'map_transition.json'], environment)
        run('retained_ballistics', [sys.executable, ROOT / 'game_shared/tests/ballistic_manager_j11_2_integration.py'],
            dict(environment, J112_UBSAN='1'))

    report = {'milestone': 'K.11.4.1.1', 'passed': sum(item['returncode'] == 0 for item in results),
              'total': len(results), 'results': results, 'sanitizers': not args.no_sanitizers,
              'source_sha256': {source: hashlib.sha256((ROOT / source).read_bytes()).hexdigest()
                               for source in sorted(set(client_sources + server_sources +
                                  ['server/physx/physx_impl.cpp', 'server/physx/physics_asset.cpp']))},
              'scope': 'Native Linux objects; CPU/GPU profiling core, GL capability adapter and actual Physics Update recording adapter; retained production-method regressions. No native Windows link, complete game link, real PhysX solver, GPU or FPS measurements.'}
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2) + '\n')
    print(f"K.11.4.1.1: {report['passed']}/{report['total']} PASS", flush=True)
    return int(report['passed'] != report['total'])


if __name__ == '__main__':
    raise SystemExit(main())
