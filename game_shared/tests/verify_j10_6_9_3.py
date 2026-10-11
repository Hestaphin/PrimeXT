#!/usr/bin/env python3
"""J.10.6.9.3 steps 3-4 checks, based on J.10.6.9.2.

Uses real shared loaders/writers and editor filesystem code. Sky method tests
adapt GPU and view services; complete translation-unit checks supplement them.
These checks do not certify a linked Windows build or live rendering.
"""
from pathlib import Path
import argparse
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / 'game_shared/tests'


def execute(name, command, environment):
    result = subprocess.run(command, cwd=ROOT, env=environment,
                            capture_output=True, text=True)
    output = (result.stdout + result.stderr).strip()
    if output:
        print(output[-30000:], flush=True)
    print(('PASS ' if result.returncode == 0 else 'FAIL ') + name, flush=True)
    return result.returncode == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan', action='store_true')
    parser.add_argument('--skip-inherited', action='store_true')
    args = parser.parse_args()
    sanitizer = ['-fsanitize=undefined', '-fno-sanitize-recover=all'] if args.ubsan else []
    inherited_flags = ['--ubsan'] if args.ubsan else []
    results = []
    with tempfile.TemporaryDirectory(prefix='j10693_verify_', dir=ROOT) as name:
        temporary = Path(name)
        environment = os.environ.copy()
        environment['TMPDIR'] = str(temporary)
        compiler = os.environ.get('CXX', 'c++')
        shared = [str(ROOT / 'game_shared/modular_weapons.cpp')]
        warning_flags = ['-Wall', '-Wextra', '-Wpedantic', '-Werror']
        includes = ['-I' + str(ROOT / part) for part in ['game_shared', 'tools/weapon_editor']]
        binary = temporary / 'format-tests'
        built = execute('shared-format/native-editor test compilation',
            [compiler, '-std=c++17', *warning_flags, *sanitizer, *includes,
             str(ROOT / 'tools/weapon_editor/tests/native_format_tests.cpp'), *shared,
             str(ROOT / 'tools/weapon_editor/pxwe_project.cpp'),
             str(ROOT / 'game_shared/ballistic_flight.cpp'), '-o', str(binary)], environment)
        results.append(built)
        results.append(built and execute('28 format, compatibility and filesystem cases',
            [str(binary), str(ROOT)], environment))

        # Both DLL configurations build the same real reader and fingerprint.
        # This is a data-handshake check, not a linked client/server executable.
        outputs = {}
        for side, defines in [('client', ['-DCLIENT_DLL', '-DCLIENT_WEAPONS']), ('server', [])]:
            validator = temporary / ('validate-' + side)
            built = execute(side + ' shared validator compilation',
                [compiler, '-std=c++17', *warning_flags, *sanitizer, *defines, *includes,
                 str(ROOT / 'tools/weapon_editor/native_validate.cpp'), *shared,
                 '-o', str(validator)], environment)
            results.append(built)
            if built:
                outputs[side] = []
                for sample in ['modular-ballistics', 'modular-format3']:
                    result = subprocess.run([str(validator),
                        str(ROOT / 'documentation/examples' / sample)], cwd=ROOT,
                        env=environment, capture_output=True, text=True)
                    outputs[side].append((result.returncode, result.stdout.strip()))
                    if result.returncode:
                        print((result.stdout + result.stderr).strip(), flush=True)
        same = (len(outputs) == 2 and outputs['client'] == outputs['server']
                and all(code == 0 for code, _ in outputs['client']))
        print(('PASS ' if same else 'FAIL ') + 'client/server legacy and format-3 fingerprints', flush=True)
        if same:
            for _, line in outputs['client']:
                print(line, flush=True)
        results.append(same)

        results.append(execute('17 sky production-method cases',
            [sys.executable, str(TESTS / 'sky_j10_6_9_3_integration.py'), *inherited_flags], environment))
        if sys.platform.startswith('linux'):
            common = [compiler, '-std=c++17', '-fsyntax-only', '-fno-exceptions',
                '-Wno-write-strings', '-Wno-invalid- offsetof'.replace(' ', ''),
                '-D_LINUX', '-DLINUX', '-DCLIENT_DLL', '-DCLIENT_WEAPONS',
                '-Dstricmp=strcasecmp', '-Dstrnicmp=strncasecmp',
                '-D_snprintf=snprintf', '-D_vsnprintf=vsnprintf',
                *['-I' + str(ROOT / part) for part in ['client', 'client/render',
                    'client/visualizer', 'client/events', 'client/ui', 'common',
                    'engine', 'game_shared', 'public']]]
            for unit in ['client/render/gl_rmain.cpp', 'client/render/gl_rmisc.cpp',
                         'client/render/gl_sky.cpp', 'client/weapon_predicting_context.cpp']:
                results.append(execute('complete syntax: ' + unit, common + [str(ROOT / unit)], environment))
        if not args.skip_inherited:
            results.append(execute('inherited J.10.6.9.2 checks',
                [sys.executable, str(TESTS / 'verify_j10_6_9_2.py'), *inherited_flags], environment))
    print(f'J.10.6.9.3 available check groups: {sum(results)}/{len(results)} passed', flush=True)
    print('Native Windows linking and live map/reflection/cubemap images require separate acceptance.', flush=True)
    return 0 if all(results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
