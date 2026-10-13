#!/usr/bin/env python3
"""J.10.6.9.7 category transactions/controller checks and inherited acceptance."""
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
    args = parser.parse_args(); results = []
    with tempfile.TemporaryDirectory(prefix='j10697_verify_', dir=ROOT) as directory:
        directory = Path(directory); environment = os.environ.copy(); environment['TMPDIR'] = str(directory)
        def check(name, command):
            run = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True, text=True)
            if run.stdout or run.stderr: print((run.stdout+run.stderr).strip()[-90000:], flush=True)
            results.append(run.returncode == 0); print(('PASS ' if results[-1] else 'FAIL ')+name, flush=True)
            return run.returncode == 0
        flags = ['-std=c++17', '-O0', '-g', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-I'+str(EDITOR), '-I'+str(ROOT/'game_shared')]
        if args.ubsan: flags += ['-fsanitize=undefined', '-fno-sanitize-recover=all']
        binary = directory/'categories'
        if check('native category compilation', [os.environ.get('CXX', 'c++'), *flags,
            str(EDITOR/'tests/native_category_tests.cpp'), str(EDITOR/'pxwe_project.cpp'), str(EDITOR/'pxwe_preview.cpp'),
            str(ROOT/'game_shared/modular_weapons.cpp'), str(ROOT/'game_shared/ballistic_flight.cpp'), '-o', str(binary)]):
            check('category state, grammar, conversions, copies, references, history and exports', [str(binary)])
        check('actual native tree/context/notification/controller methods with OS adapters',
            [sys.executable, str(ROOT/'game_shared/tests/pxwe_j10_6_9_7_integration.py'), *(['--ubsan'] if args.ubsan else [])])
        source=(EDITOR/'pxwe_win32.cpp').read_text(); cmake=(EDITOR/'CMakeLists.txt').read_text(); rc=(EDITOR/'pxwe.rc').read_text()
        version=ET.parse(EDITOR/'pxwe.manifest').getroot().find('{urn:schemas-microsoft-com:asm.v1}assemblyIdentity').get('version')
        wired = version in {'1.0.9.7','1.0.9.8','1.0.9.9','1.11.0.0'} and ('FILEVERSION '+version.replace('.',',')) in rc and rc.count('RT_MANIFEST "pxwe.manifest"')==1
        wired &= all(x in cmake for x in ['pxwe-category-tests tests/native_category_tests.cpp','/MANIFEST:NO','gdi32 winmm'])
        wired &= all(x in source for x in ['case WM_CONTEXTMENU:', 'app->TreeContext(', 'TVN_KEYDOWN', 'VK_F2',
            'PostMessageW(window,WM_COMMAND,CMD_REFRESH_TREE,0)', 'case CMD_REFRESH_TREE:', 'project.DuplicateCategory(', 'project.RemoveCategory('])
        results.append(wired); print(('PASS ' if wired else 'FAIL ')+'native category source/resource wiring (not an SDK link)',flush=True)
        if not args.skip_inherited:
            check('inherited 9.6 previews / 9.5 usability / runtime / sky / physics acceptance',
                [sys.executable,str(ROOT/'game_shared/tests/verify_j10_6_9_6.py'), *(['--ubsan'] if args.ubsan else [])])
    print(f'J.10.6.9.7 available check groups: {sum(results)}/{len(results)} passed',flush=True)
    print('Native Windows SDK linking, real controls/message ordering/DPI/painting/audio and live game acceptance require a native build/run.',flush=True)
    return 0 if all(results) else 1

if __name__=='__main__': raise SystemExit(main())
