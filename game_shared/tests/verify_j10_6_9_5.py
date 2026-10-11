#!/usr/bin/env python3
"""Step-6 native weapon editor checks and inherited J.10.6.9.4 acceptance.

Portable C++ helpers and real controller methods are checked. Windows SDK
linking, native painting and live game acceptance are separate requirements.
"""
from pathlib import Path
import argparse
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
EDITOR = ROOT/'tools/weapon_editor'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan', action='store_true')
    parser.add_argument('--skip-inherited', action='store_true')
    args = parser.parse_args()
    results = []
    with tempfile.TemporaryDirectory(prefix='j10695_verify_', dir=ROOT) as directory:
        directory = Path(directory); environment = os.environ.copy(); environment['TMPDIR'] = str(directory)
        def check(name, command):
            run = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True, text=True)
            if run.stdout or run.stderr: print((run.stdout+run.stderr).strip()[-40000:], flush=True)
            results.append(run.returncode == 0)
            print(('PASS ' if results[-1] else 'FAIL ') + name, flush=True)
            return run.returncode == 0
        flags = ['-std=c++17', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                 '-I'+str(EDITOR), '-I'+str(ROOT/'game_shared')]
        if args.ubsan: flags += ['-fsanitize=undefined', '-fno-sanitize-recover=all', '-g']
        binary = directory/'usability'
        built = check('native property/usability compilation', [os.environ.get('CXX', 'c++'), *flags,
            str(EDITOR/'tests/native_usability_tests.cpp'), str(EDITOR/'pxwe_properties.cpp'),
            str(EDITOR/'pxwe_project.cpp'), str(ROOT/'game_shared/modular_weapons.cpp'),
            str(ROOT/'game_shared/ballistic_flight.cpp'), '-o', str(binary)])
        if built: check('native property/filesystem/history behavior', [str(binary), str(ROOT)])
        check('real Win32 editor controller methods with control adapters',
              [sys.executable, str(ROOT/'game_shared/tests/pxwe_j10_6_9_5_integration.py'), *(['--ubsan'] if args.ubsan else [])])
        cmake = (EDITOR/'CMakeLists.txt').read_text()
        rc = (EDITOR/'pxwe.rc').read_text()
        manifest = ET.parse(EDITOR/'pxwe.manifest')
        wiring = 'pxwe_project.cpp pxwe_properties.cpp' in cmake and 'comctl32 comdlg32' in cmake
        wiring &= 'add_executable(pxwe WIN32 pxwe_win32.cpp pxwe.rc pxwe_resource.h)' in cmake
        wiring &= 'target_link_options(pxwe PRIVATE /MANIFEST:NO)' in cmake and rc.count('RT_MANIFEST "pxwe.manifest"') == 1
        wiring &= manifest.getroot().find('{urn:schemas-microsoft-com:asm.v1}assemblyIdentity').get('version') in {'1.0.9.5', '1.0.9.6', '1.0.9.7'}
        wiring &= 'pxwe-usability-tests tests/native_usability_tests.cpp' in cmake
        results.append(wiring); print(('PASS ' if wiring else 'FAIL ')+'source build/resource wiring (not a Windows link)', flush=True)
        if not args.skip_inherited:
            check('inherited J.10.6.9.4 runtime, format, sky, editor and earlier acceptance',
                  [sys.executable, str(ROOT/'game_shared/tests/verify_j10_6_9_4.py'), *(['--ubsan'] if args.ubsan else [])])
    print(f'J.10.6.9.5 available check groups: {sum(results)}/{len(results)} passed', flush=True)
    print('Windows x86 SDK/resource linking and GUI painting at native DPI still require a native build/run.', flush=True)
    return 0 if all(results) else 1

if __name__ == '__main__': raise SystemExit(main())
