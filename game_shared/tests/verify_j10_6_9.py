#!/usr/bin/env python3
"""Build/run the real C++ pxwe backend and CLI, then inherited acceptance.

The Windows GUI/resource build and native visual behavior are separate checks.
No Python is invoked by pxwe itself. This script is developer-only.
"""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan', action='store_true')
    parser.add_argument('--skip-inherited', action='store_true')
    args = parser.parse_args()
    compiler = os.environ.get('CXX', 'c++')
    passed = total = 0
    environment = os.environ.copy()

    def run(command, cwd=ROOT, expected=0):
        result = subprocess.run(command, cwd=cwd, env=environment, capture_output=True, text=True)
        output = (result.stdout + result.stderr).strip()
        if output:
            print(output[-30000:], flush=True)
        if result.returncode != expected:
            raise RuntimeError(f'exit {result.returncode}; expected {expected}')
        return result

    def check(name, action):
        nonlocal passed, total
        total += 1
        try:
            action()
            passed += 1
            print('PASS ' + name, flush=True)
        except (OSError, RuntimeError, AssertionError) as error:
            print('FAIL ' + name + ': ' + str(error), flush=True)

    with tempfile.TemporaryDirectory(prefix='j1069_native_') as temporary:
        temporary = Path(temporary)
        environment['TMPDIR'] = str(temporary)
        flags = ['-std=c++17', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                 '-I' + str(ROOT / 'game_shared'), '-I' + str(ROOT / 'tools/weapon_editor')]
        if args.ubsan:
            flags += ['-fsanitize=undefined', '-fno-sanitize-recover=all', '-g']
        core = [str(ROOT / 'tools/weapon_editor/pxwe_project.cpp'),
                str(ROOT / 'tools/weapon_editor/pxwe_preview.cpp'),
                str(ROOT / 'game_shared/modular_weapons.cpp'), str(ROOT / 'game_shared/ballistic_flight.cpp')]
        tests = temporary / 'pxwe-tests'
        cli = temporary / 'pxwe'
        check('native backend build and tests against existing example packs', lambda: (
            run([compiler, *flags, *core, str(ROOT / 'tools/weapon_editor/tests/native_project_tests.cpp'), '-o', str(tests)]),
            run([str(tests), str(ROOT)])))
        check('portable native pxwe build', lambda: run([
            compiler, *flags, *core, str(ROOT / 'tools/weapon_editor/pxwe_cli.cpp'), '-o', str(cli)]))

        def source_free():
            game = temporary / 'installed-game'
            shutil.copytree(ROOT / 'documentation/examples/modular-ballistics', game)
            (game / 'devkit').mkdir()
            installed = game / 'devkit/pxwe'
            shutil.copy2(cli, installed)
            unrelated_cwd = temporary / 'shortcut-cwd'
            unrelated_cwd.mkdir()
            result = run([str(installed), '--validate', str(game)], cwd=unrelated_cwd)
            assert 'Script-valid:' in result.stdout and 'Fingerprint:' in result.stdout
            run([str(installed), '--version'], cwd=unrelated_cwd)
            cartridge = game / 'scripts/cartridges/test_jhp.cartridge'
            text = cartridge.read_text()
            assert 'damage = 18' in text
            cartridge.write_text(text.replace('damage = 18', 'damage = nan'))
            before = {p.relative_to(game): p.read_bytes() for p in game.rglob('*') if p.is_file()}
            run([str(installed), '--validate', str(game)], cwd=unrelated_cwd, expected=1)
            assert before == {p.relative_to(game): p.read_bytes() for p in game.rglob('*') if p.is_file()}
            assert not list(game.rglob('*.py')) and not (game / 'game_shared').exists()

        check('installed native CLI validates without a source tree and rejects bad data', source_free)
    environment = os.environ.copy()
    if not args.skip_inherited:
        check('inherited J.10.6.8 acceptance', lambda: run([
            sys.executable, str(ROOT / 'game_shared/tests/verify_j10_6_8.py'),
            *(['--ubsan'] if args.ubsan else [])]))
    print(f'J.10.6.9 available check groups: {passed}/{total} passed', flush=True)
    print('Windows GUI/resource linking, CMake target builds and native visual acceptance require a separate environment.', flush=True)
    return 0 if passed == total else 1


if __name__ == '__main__':
    raise SystemExit(main())
