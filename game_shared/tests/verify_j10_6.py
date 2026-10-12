#!/usr/bin/env python3
"""J.10.6: replay SDK-independent gameplay, ballistics and source checks.

Run from any directory: python3 game_shared/tests/verify_j10_6.py
Requires Python 3 and a C++17 compiler. This is NOT a replacement for the
vcpkg/PhysX-enabled build or live engine acceptance tests.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / 'game_shared' / 'tests'
SHARED = ROOT / 'game_shared'
CXX = os.environ.get('CXX', 'c++')

BALLISTICS = ['ballistic_flight.cpp', 'ballistic_penetration.cpp', 'ballistic_path_cache.cpp']
TEST_CASES = [
    ('modular_weapons_test', ['modular_weapons.cpp'], 'documentation/examples/modular-weapon'),
    ('modular_j4_test', ['modular_weapons.cpp'], 'game_shared/tests/fixtures/j4'),
    ('modular_j6_test', ['modular_weapons.cpp'], 'documentation/examples/modular-ballistics'),
    ('modular_j7_integration_test', ['ballistic_flight.cpp'], None),
    ('ballistic_j8_tests', BALLISTICS, None),
    ('ballistic_j9_tests', BALLISTICS, None),
    ('ballistic_j10_1_tests', BALLISTICS, None),
    ('ballistic_j10_2_tests', BALLISTICS, None),
    ('modular_j10_3_tests', [], None),
    ('physics_collision_prediction_j10_4_tests', [], None),
    ('modular_j10_5_identity_tests', [], None),
    ('modular_j10_6_shot_storage_tests', [], None),
    ('physics_asset_i1_tests', ['physics_asset_format.cpp'], None),
    ('physics_asset_trace_owner_i1010_tests', [], None),
]

def launch(args, *, quiet=False):
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True)
    if result.returncode or not quiet:
        output = (result.stdout + result.stderr).strip()
        if output:
            print(output[-1600:])
    return result.returncode == 0

def main():
    total = passed = 0
    with tempfile.TemporaryDirectory(prefix='primext_j106_') as tmp:
        for name, shared, fixture in TEST_CASES:
            total += 1
            binary = str(Path(tmp) / name)
            compile_args = [CXX, '-std=c++17', '-O0', '-I' + str(SHARED),
                            str(TESTS / (name + '.cpp')),
                            *[str(SHARED / src) for src in shared], '-o', binary]
            args = [binary]
            if fixture:
                args.append(str(ROOT / fixture))
            ok = launch(compile_args, quiet=True) and launch(args, quiet=True)
            print(('PASS' if ok else 'FAIL') + ' C++ ' + name)
            passed += ok
    for script in sorted(TESTS.glob('*tests.py')):
        total += 1
        ok = launch([sys.executable, str(script)], quiet=True)
        print(('PASS' if ok else 'FAIL') + ' source ' + script.name)
        passed += ok
    print(f'J.10.6 portable/source checks: {passed}/{total} passed')
    return 0 if passed == total else 1

if __name__ == '__main__':
    raise SystemExit(main())
