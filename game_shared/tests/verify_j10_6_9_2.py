#!/usr/bin/env python3
"""J.10.6.9.2 behavioral, changed-translation-unit and inherited checks.

Xash C syntax, when its dependency headers are available, uses an explicitly
supplied portable build.h from PrimeXT. Missing dependency checks are reported
as BLOCKED, separately from executed checks. This is not an engine link/build.
Windows x86 linking and live-map acceptance still require a native environment.
"""
from pathlib import Path
import argparse
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / 'game_shared/tests'
XASH = ROOT.parent / 'xash3d-fwgs-master'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan', action='store_true')
    parser.add_argument('--skip-inherited', action='store_true')
    args = parser.parse_args()
    flags = ['--ubsan'] if args.ubsan else []
    blocked = []
    checks = [('HUD/entity-tool/callback/placement method cases', [sys.executable,
        str(TESTS / 'hud_enttools_j10_6_9_2_integration.py'), *flags])]
    with tempfile.TemporaryDirectory(prefix='j10692_build_header_', dir=ROOT) as temporary:
        temporary = Path(temporary)
        (temporary / 'build.h').write_bytes((ROOT / 'public/build.h').read_bytes())
        environment = os.environ.copy()
        # Native std::filesystem tests need an existing temporary directory;
        # some restricted environments do not provide /tmp.
        environment['TMPDIR'] = str(temporary)
        if sys.platform.startswith('linux'):
            common = [os.environ.get('CXX', 'c++'), '-std=c++17', '-fsyntax-only', '-fno-exceptions',
                '-Wno-write-strings', '-Wno-invalid- offsetof'.replace(' ', ''),
                '-D_LINUX', '-DLINUX', '-Dstricmp=strcasecmp', '-Dstrnicmp=strncasecmp',
                '-D_snprintf=snprintf', '-D_vsnprintf=vsnprintf']
            for name, defines, includes in [
                ('client/ammo.cpp', ['-DCLIENT_DLL', '-DCLIENT_WEAPONS'],
                    ['client', 'client/render', 'client/visualizer', 'client/events', 'client/ui',
                     'common', 'engine', 'game_shared', 'public']),
                ('server/saverestore.cpp', [], ['server', 'server/gamerules',
                    'common', 'engine', 'game_shared', 'public']),
            ]:
                checks.append(('PrimeXT ' + name + ' complete syntax', common + defines +
                    ['-I' + str(ROOT / item) for item in includes] + [str(ROOT / name)]))
            engine = [os.environ.get('CC', 'cc'), '-std=c99', '-fsyntax-only', '-D_GNU_SOURCE',
                '-DXASH_TIMER=2', '-DXASH_VIDEO=3', '-DXASH_SOUND=0', '-DXASH_INPUT=0',
                '-DXASH_LIB=1', '-DXASH_AVI=0', '-I' + str(temporary),
                *['-I' + str(XASH / part) for part in ['engine/common', 'common', 'engine',
                    'engine/server', 'engine/client', 'engine/platform', 'public', 'filesystem']]]
            for name in ['engine/server/sv_client.c', 'engine/server/sv_game.c', 'engine/client/cl_main.c']:
                label = 'Xash ' + name + ' complete syntax (portable build.h adapter)'
                suffix = XASH / '3rdparty/library_suffix/library_suffix.h'
                if not suffix.is_file():
                    blocked.append(label + ': supplied source lacks library_suffix.h dependency')
                else:
                    checks.append((label, engine + ['-I' + str(suffix.parent), str(XASH / name)]))
        if not args.skip_inherited:
            checks.append(('inherited J.10.6.9 native-editor/game acceptance', [sys.executable,
                str(TESTS / 'verify_j10_6_9.py'), *flags]))
        passed = 0
        for name, command in checks:
            result = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True, text=True)
            output = (result.stdout + result.stderr).strip()
            if output: print(output[-25000:], flush=True)
            print(('PASS ' if not result.returncode else 'FAIL ') + name, flush=True)
            passed += result.returncode == 0
    print(f'J.10.6.9.2 available check groups: {passed}/{len(checks)} passed', flush=True)
    for reason in blocked: print('BLOCKED ' + reason, flush=True)
    print('Native Windows linking and live-map pickup/visual acceptance remain separate checks.', flush=True)
    return 0 if passed == len(checks) else 1


if __name__ == '__main__':
    raise SystemExit(main())
