#!/usr/bin/env python3
"""J.10.6.10 / F0: portable measurements, actual recorder methods and scope.

Requires Python 3 and a GCC-compatible C++17 compiler. --ubsan also instruments
the production matrix/math and recorder code. --inherited runs the complete
prior verification chain once. SDK linking and live acceptance remain separate.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / 'game_shared/tests'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan', action='store_true')
    parser.add_argument('--inherited', action='store_true')
    parser.add_argument('--skip-baseline-scope', action='store_true',
                        help='For later phases only: retain recorder checks but skip F0-only unchanged-physics hashes')
    args = parser.parse_args()
    results = []
    def check(name, command):
        run = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        if run.stdout or run.stderr: print((run.stdout + run.stderr).strip()[-12000:], flush=True)
        ok = run.returncode == 0
        results.append(ok)
        print(('PASS ' if ok else 'FAIL ') + name, flush=True)
        return ok
    includes = ['game_shared', 'public', 'common', 'engine']
    flags = ['-std=c++17', '-O0', '-g', '-D_LINUX', '-DLINUX', *['-I' + str(ROOT / p) for p in includes]]
    if args.ubsan: flags += ['-fsanitize=undefined', '-fno-sanitize-recover=all']
    strict = ['-Wall', '-Wextra', '-Werror', '-Wno-deprecated-copy'] # existing project vector/matrix copy declarations
    with tempfile.TemporaryDirectory(prefix='ragdoll_f0_') as tmp:
        tmp = Path(tmp)
        objects = []
        for library in ('matrix', 'mathlib'):
            obj = tmp / (library + '.o')
            if check('production ' + library + ' compilation', [os.environ.get('CXX', 'c++'), *flags,
                      '-c', str(ROOT / 'game_shared' / (library + '.cpp')), '-o', str(obj)]): objects.append(str(obj))
        if len(objects) == 2:
            binary = tmp / 'measurements'
            if check('measurement and Studio-link compilation', [os.environ.get('CXX', 'c++'), *flags, *strict,
                     '-Wno-unknown-pragmas', '-Wno-unused-variable', # existing vector.h MSVC pragmas/ReverseSqrt local
                     str(TESTS / 'ragdoll_f0_tests.cpp'), *objects, '-o', str(binary)]):
                check('measurement math, invalid values, cadence, peaks and Studio link regressions', [str(binary)])
        adapter = tmp / 'adapter'
        if check('production PhysX getter-adapter compilation (facade, NOT SDK)',
                 [os.environ.get('CXX', 'c++'), *flags, *strict, '-Wpedantic',
                  '-I' + str(TESTS / 'fixtures/j10_6_10'), str(TESTS / 'ragdoll_f0_physx_adapter_tests.cpp'), '-o', str(adapter)]):
            check('rotated anchors, angular modes, velocities, broken joints and scene counters', [str(adapter)])
        check('actual runtime and pxmv recording methods, preview enabled/disabled',
              [sys.executable, str(TESTS / 'ragdoll_f0_integration.py'), *(['--ubsan'] if args.ubsan else [])])

    if args.skip_baseline_scope:
        print('SKIP F0-only unchanged-physics hashes: later phase explicitly changes the generator and angular-motion fields.', flush=True)
    else:
        # This fixture records actual baseline bytes, not desired new constants.
        baseline = json.loads((ROOT / 'documentation/milestones/J.10.6.10/F0_BASELINE.json').read_text())
        mismatches = []
        for filename, expected in baseline['unchanged_runtime_files'].items():
            if hashlib.sha256((ROOT / filename).read_bytes()).hexdigest() != expected: mismatches.append(filename)
        from ragdoll_f0_integration import function
        for entry in baseline['unchanged_physics_functions']:
            text = (ROOT / entry['file']).read_text()
            actual = hashlib.sha256(function(text, entry['signature']).encode()).hexdigest()
            if actual != entry['sha256']: mismatches.append(entry['signature'])
        results.append(not mismatches)
        print(('PASS' if not mismatches else 'FAIL') + ' F0 unchanged format, filtering, generator, actor/joint construction and CPU reconstruction' +
              (': ' + ', '.join(mismatches) if mismatches else ''), flush=True)
    if args.inherited:
        check('inherited portable, source, method and syntax acceptance',
              [sys.executable, str(TESTS / 'verify_j10_6_9_7.py'), *(['--ubsan'] if args.ubsan else [])])
    print(f'J.10.6.10 F0 available groups: {sum(results)}/{len(results)} passed', flush=True)
    print('Native PhysX/Windows links, GUI, matching Barney reproduction, live game and FPS are NOT certified.', flush=True)
    return 0 if all(results) else 1

if __name__ == '__main__': raise SystemExit(main())
