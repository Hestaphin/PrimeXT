#!/usr/bin/env python3
"""K.11.4.1.2 native objects and production-method cache/geometry adapters.

Use --retained to also run the profiling, renderer, map-transition and ballistic
regression suites. No full game link, PhysX solver, real GPU or FPS benchmark.
"""
import argparse
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
    parser.add_argument('--retained', action='store_true')
    args = parser.parse_args()
    temporary = tempfile.TemporaryDirectory(prefix='primext-pose-k11412-') if not args.work_dir else None
    work = args.work_dir.resolve() if args.work_dir else Path(temporary.name)
    work.mkdir(parents=True, exist_ok=True)
    compiler = shlex.split(os.environ.get('CXX', 'g++'))
    results = []

    def run(name, command):
        result = subprocess.run([str(value) for value in command], cwd=ROOT, capture_output=True, text=True)
        (work / (name + '.log')).write_text(result.stdout + result.stderr)
        results.append({'name': name, 'returncode': result.returncode})
        print(name, 'PASS' if result.returncode == 0 else 'FAIL', flush=True)
        if result.returncode:
            print((result.stdout + result.stderr)[-6500:], flush=True)
        return result.returncode == 0

    impl = (ROOT / 'server/physx/physx_impl.cpp').read_text()
    header = (ROOT / 'server/physx/physx_impl.h').read_text()
    pose = (ROOT / 'server/studio_pose_cache.cpp').read_text()
    client = (ROOT / 'client/utils.cpp').read_text()
    structures = '\n'.join(function(header, 'struct ' + name + '\n') + ';' for name in
        ['PhysicsPartShape', 'PhysicsPartRuntime', 'PhysicsAssetCollisionState', 'ImpactPoseCache'])
    structures += '\nstd::map<int, ImpactPoseCache> m_impactPoseMeshes;\nstd::uint64_t m_impactPoseTouch = 0;\n'
    methods = '\n'.join(function(impl, name) for name in
        ['static mstudiobodyparts_t *FindBodyPartByName', 'static int ResolveBodygroupVariant',
         'static bool CollisionSectionActive', 'static int FindStudioBoneIndex',
         'static PxTransform StudioBoneWorldTransform',
         'void CPhysicPhysX::RebuildAnimatedCollisionTraceMesh',
         'static bool BuildCurrentStudioRenderMesh', 'bool CPhysicPhysX::ResolveRenderImpact'])
    client_globals = client[client.index('static std::unordered_map<std::string, std::shared_ptr<physicsasset::AssetDef>>'):
                            client.index('static const physicsasset::AssetDef *PredictionPhysicsAsset')]
    client_methods = client_globals + '\n'.join(function(client, name) for name in
        ['static bool PredictionCollisionActive', 'static int PredictionBoneIndex',
         'static bool PredictionHasPrimitiveHinges', 'static bool BuildPredictedPhysicsMesh',
         'static CMeshDesc *CachedPredictedPhysicsMesh'])
    fixture = (ROOT / 'game_shared/tests/fixtures/pose_cache_k11412.cpp.in').read_text()
    generated = fixture.replace('@STRUCTS@', structures).replace('@SERVER_POSE_CACHE@', pose[pose.index('namespace\n{'):])
    generated = generated.replace('@SERVER_METHODS@', methods).replace('@CLIENT_METHODS@', client_methods)
    instance = (ROOT / 'server/physx/physics_asset.cpp').read_text()
    generated = generated.replace('@INSTANCE_METHODS@', '\n'.join(function(instance, name) for name in
        ['PxTransform PhysicsAssetInstance::BodyLocalTransform', 'PxTransform PhysicsAssetInstance::BoneWorldTransform']))
    # Freeze the reviewed K.11.4.1.1 implementation in a fixture; the verifier
    # works on an applied patch checkout without a Git history or old tag.
    baseline = (ROOT / 'game_shared/tests/fixtures/pose_cache_k11412_baseline.cpp.in').read_text()
    generated = generated.replace('@BASELINE_METHODS@', baseline)
    unit = work / 'cache_adapter.cpp'
    unit.write_text(generated)
    definitions = ['-D_LINUX=1', '-DLINUX=1', '-D_vsnprintf=vsnprintf', '-D_snprintf=snprintf',
                   '-Dstricmp=strcasecmp', '-Dstrnicmp=strncasecmp']
    flags = ['-std=c++17', '-fno-exceptions', '-Wno-write-strings', '-ffunction-sections', '-fdata-sections', *definitions]
    shared_include = ['-I' + str(ROOT / path) for path in ['game_shared', 'common', 'engine', 'public']]
    math_sources = [ROOT / 'game_shared' / name for name in ['matrix.cpp', 'vector.cpp', 'mathlib.cpp', 'stringlib.cpp']]
    for optimization in ['O0', 'O2']:
        executable = work / ('cache_' + optimization)
        if run('adapter_compile_' + optimization, [*compiler, *flags, '-' + optimization, '-g',
            '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-pie', '-no-pie', *shared_include,
            unit, *math_sources, ROOT / 'game_shared/performance_profile.cpp', '-Wl,--gc-sections', '-o', executable]):
            run('adapter_run_' + optimization, [executable])
            run('adapter_disabled_' + optimization, [executable, 'profiling_off'])

    server_include = ['-I' + str(ROOT / path) for path in
        ['server', 'server/monsters', 'server/weapons', 'server/entities', 'server/gamerules']]
    client_include = ['-I' + str(ROOT / path) for path in ['client', 'client/render', 'client/ui']]
    for optimization in ['O0', 'O2']:
        for source, includes, defines in [
            ('server/studio_pose_cache.cpp', server_include, []),
            ('server/client.cpp', server_include, []),
            ('server/cbase.cpp', server_include, []),
            ('client/utils.cpp', client_include, ['-DCLIENT_DLL=1', '-DCLIENT_WEAPONS=1'])]:
            name = f"native_{optimization}_{source.replace('/', '_').replace('.cpp', '')}"
            run(name, [*compiler, *flags, '-' + optimization, '-Wno-invalid-offsetof',
                '-Wno-pointer-arith', *includes, *shared_include, *defines, '-c', ROOT / source, '-o', work / (name + '.o')])

    for name in ['physics_asset_trace_i102_source_tests.py', 'physics_asset_primitive_i104_source_tests.py',
                 'physics_asset_collision_ownership_i92_source_tests.py', 'physics_asset_trace_owner_i1010_source_tests.py',
                 'physics_asset_cache_lifecycle_i107_source_tests.py', 'physics_part_restore_i109_source_tests.py']:
        run(Path(name).stem, [sys.executable, ROOT / 'game_shared/tests' / name])
    for name in ['physics_asset_trace_scale_i102_tests.cpp', 'physics_asset_primitive_i104_tests.cpp',
                 'physics_asset_trace_owner_i1010_tests.cpp']:
        executable = work / Path(name).stem
        if run(Path(name).stem + '_compile', [*compiler, *flags, '-O2', '-g',
            '-fsanitize=undefined', '-fno-sanitize-recover=all', *shared_include,
            ROOT / 'game_shared/tests' / name, *math_sources, '-Wl,--gc-sections', '-o', executable]):
            run(Path(name).stem + '_run', [executable])
    if args.retained:
        run('retained_profiling_and_regressions', [sys.executable, ROOT / 'game_shared/tests/verify_performance_k11411.py',
            '--work-dir', work / 'retained', '--json', work / 'retained.json'])
    report = {'milestone': 'K.11.4.1.2', 'passed': sum(item['returncode'] == 0 for item in results),
              'total': len(results), 'results': results,
              'scope': 'Native Linux client/utils and server cache/client/cbase objects; extracted production bone/cache/mesh methods with real project matrix and primitive geometry under ASan/UBSan. No native PhysX SDK link, complete game link, GPU or FPS measurement.'}
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2) + '\n')
    print(f"K.11.4.1.2: {report['passed']}/{report['total']} PASS", flush=True)
    return int(report['passed'] != report['total'])


if __name__ == '__main__':
    raise SystemExit(main())
