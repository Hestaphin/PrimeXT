#!/usr/bin/env python3
"""Run production activation/setup regressions with explicit recording services.
No native PhysX/GPU linkage or in-game FPS claim.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
from pose_j10_6_2_integration import function

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / 'game_shared/tests'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--json', type=Path)
    parser.add_argument('--original-root', type=Path)
    parser.add_argument('--baseline-root', type=Path)
    args = parser.parse_args()
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    results = []
    def run(label, command):
        result = subprocess.run([str(x) for x in command], cwd=ROOT, capture_output=True, text=True)
        (work / (label + '.log')).write_text(result.stdout + result.stderr)
        results.append({'test': label, 'returncode': result.returncode})
        print(label, 'PASS' if result.returncode == 0 else 'FAIL', flush=True)
        if result.returncode:
            print((result.stdout + result.stderr)[-5000:], flush=True)
        elif result.stdout:
            print(result.stdout.strip(), flush=True)
        return result.returncode == 0
    asset = (ROOT / 'server/physx/physics_asset.cpp').read_text()
    names = ['bool FiniteVec3', 'PxVec3 AngularVelocityBetween',
        'PhysicsAssetInstance::PhysicsAssetInstance', 'PhysicsAssetInstance::~PhysicsAssetInstance',
        'void PhysicsAssetInstance::InvalidateAnimationHistory', 'void PhysicsAssetInstance::CalculateAnimationVelocities',
        'void PhysicsAssetInstance::UpdateAnimationPose', 'bool PhysicsAssetInstance::CreateActors',
        'void PhysicsAssetInstance::DestroyActors', 'void PhysicsAssetInstance::Destroy()',
        'void PhysicsAssetInstance::SnapToAnimationPose', 'void PhysicsAssetInstance::SetSimulationEnabled',
        'bool PhysicsAssetInstance::SetRagdoll']
    production = '\n\n'.join(function(asset, name) for name in names)
    activation = (TESTS / 'fixtures/k11_4_3/activation.cpp.in').read_text().replace('@PRODUCTION@', production)
    unit = work / 'activation.cpp'
    unit.write_text(activation)
    flags = ['-std=c++17', '-g', '-fno-exceptions', '-fno-pie', '-no-pie',
        '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
        '-Wno-misleading-indentation', '-Wno-unused-parameter']
    flags += ['-I' + str(ROOT / path) for path in ['game_shared/tests/fixtures/j10_6_10_3',
        'game_shared/tests/fixtures/j10_6_10_1', 'game_shared', 'server/physx']]
    # LeakSanitizer requires process interfaces unavailable in the runner.
    # ASan/UBSan remain active. Setup additionally checks allocation balance.
    with unit.open('a') as stream:
        stream.write('\nextern "C" const char *__asan_default_options(){return "detect_leaks=0:halt_on_error=1";}\n')
    for config, opt in [('debug', '-O0'), ('release', '-O2')]:
        binary = work / ('activation_' + config)
        if run('activation_' + config + '_compile', [os.environ.get('CXX', 'c++'), *flags, opt,
            unit, ROOT / 'game_shared/physics_asset_format.cpp', '-o', binary]):
            run('activation_' + config, [binary, ROOT / 'documentation/examples/ragdoll_templates_f5/barney_f5_human_loose.phys'])
    if args.baseline_root:
        baseline = (args.baseline_root.resolve() / 'server/physx/physics_asset.cpp').read_text()
        production = '\n\n'.join(function(baseline, name) for name in names)
        unit = work / 'activation_baseline.cpp'
        unit.write_text((TESTS / 'fixtures/k11_4_3/activation.cpp.in').read_text().replace('@PRODUCTION@', production) +
            '\nextern "C" const char *__asan_default_options(){return "detect_leaks=0:halt_on_error=1";}\n')
        binary = work / 'activation_baseline'
        if run('activation_baseline_compile', [os.environ.get('CXX', 'c++'), *flags, '-O2',
            '-DPERFORMANCE_BASELINE=1', unit, ROOT / 'game_shared/physics_asset_format.cpp', '-o', binary]):
            run('activation_baseline', [binary, ROOT / 'documentation/examples/ragdoll_templates_f5/barney_f5_human_loose.phys'])
    template = (TESTS / 'fixtures/k11_4_3/setup.cpp.in').read_text()
    roots = [('current', ROOT)]
    if args.original_root:
        roots.append(('original', args.original_root.resolve()))
    for label, root in roots:
        impl = (root / 'server/physx/physx_impl.cpp').read_text()
        setup = '\n\n'.join(function(impl, name) for name in ['void CPhysicPhysX :: InitPhysic', 'void CPhysicPhysX :: FreePhysic'])
        unit = work / ('setup_' + label + '.cpp')
        unit.write_text(template.replace('@PRODUCTION@', setup) +
            '\nextern "C" const char *__asan_default_options(){return "detect_leaks=0:halt_on_error=1";}\n')
        for config, opt in [('debug', '-O0'), ('release', '-O2')]:
            binary = work / ('setup_' + label + '_' + config)
            if run(binary.name + '_compile', [os.environ.get('CXX', 'c++'), *flags, opt, unit, '-o', binary]):
                run(binary.name, [binary])
    run('preview_runtime_pcm', ['python3', TESTS / 'performance_scene_source_k1143.py'])
    run('contact_construction_ccd', ['python3', TESTS / 'ragdoll_f3_physx_integration.py', '--ubsan'])
    run('death_weights_impulses', ['python3', TESTS / 'physics_j10_6_3_integration.py'])
    summary = {'milestone': 'K.11.4.3', 'results': results,
        'passed': sum(r['returncode'] == 0 for r in results), 'total': len(results),
        'scope': 'Production methods and headers, real .phys parser, recording SDK/engine services. No native solver, DLL link, GPU or in-game FPS test.'}
    if args.json:
        args.json.write_text(json.dumps(summary, indent=2) + '\n')
    print(f"K.11.4.3 performance checks: {summary['passed']}/{summary['total']} PASS", flush=True)
    return int(summary['passed'] != summary['total'])

if __name__ == '__main__':
    raise SystemExit(main())
