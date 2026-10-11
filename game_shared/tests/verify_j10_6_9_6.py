#!/usr/bin/env python3
"""J.10.6.9.6 steps 7/8 native editor checks and inherited acceptance."""
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
    parser.add_argument('--preview-output', type=Path)
    args = parser.parse_args(); results = []
    with tempfile.TemporaryDirectory(prefix='j10696_verify_', dir=ROOT) as directory:
        directory = Path(directory); environment = os.environ.copy(); environment['TMPDIR'] = str(directory)
        def check(name, command):
            run = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True, text=True)
            if run.stdout or run.stderr: print((run.stdout+run.stderr).strip()[-60000:], flush=True)
            results.append(run.returncode == 0); print(('PASS ' if results[-1] else 'FAIL ')+name, flush=True)
            return run.returncode == 0
        flags = ['-std=c++17', '-O0', '-g', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-I'+str(EDITOR), '-I'+str(ROOT/'game_shared')]
        if args.ubsan: flags += ['-fsanitize=undefined', '-fno-sanitize-recover=all']
        binary = directory/'previews'
        built = check('native resource/asset/comparison compilation', [os.environ.get('CXX', 'c++'), *flags,
            str(EDITOR/'tests/native_preview_tests.cpp'), str(EDITOR/'pxwe_preview.cpp'), str(EDITOR/'pxwe_properties.cpp'),
            str(EDITOR/'pxwe_project.cpp'), str(ROOT/'game_shared/modular_weapons.cpp'),
            str(ROOT/'game_shared/ballistic_flight.cpp'), '-o', str(binary)])
        if built: check('native resource bounds, textures, animation, sprites, transactions and comparisons',
            [str(binary), *([str(args.preview_output.resolve())] if args.preview_output else [])])
        check('actual editor controller editing / page / playback / curve handlers',
            [sys.executable, str(ROOT/'game_shared/tests/pxwe_j10_6_9_6_integration.py'), *(['--ubsan'] if args.ubsan else [])])
        cmake = (EDITOR/'CMakeLists.txt').read_text(); source = (EDITOR/'pxwe_win32.cpp').read_text(); rc = (EDITOR/'pxwe.rc').read_text()
        version = ET.parse(EDITOR/'pxwe.manifest').getroot().find('{urn:schemas-microsoft-com:asm.v1}assemblyIdentity').get('version')
        wired = all(item in cmake for item in ['pxwe_preview.cpp', 'gdi32 winmm', 'pxwe-preview-tests tests/native_preview_tests.cpp', '/MANIFEST:NO'])
        wired &= all(item in source for item in ['L"Sprites"', 'WM_TIMER', 'app->Tick()', 'StretchDIBits(', 'studio.Render(', 'sprite.Render(', 'case C_COEFFICIENT:', 'case C_OVERRIDES:'])
        wired &= version in {'1.0.9.6','1.0.9.7'} and rc.count('RT_MANIFEST "pxwe.manifest"') == 1 and ('FILEVERSION '+version.replace('.',',')) in rc
        wired &= all(item in rc for item in ['IDD_VARIANT DIALOGEX', 'IDD_OVERRIDES DIALOGEX', 'IDC_VARIANT_WHEN'])
        results.append(wired); print(('PASS ' if wired else 'FAIL ')+'native build/resource source wiring (not an SDK link)', flush=True)
        if not args.skip_inherited:
            check('inherited J.10.6.9.5 / runtime / sky / editor acceptance',
                [sys.executable, str(ROOT/'game_shared/tests/verify_j10_6_9_5.py'), *(['--ubsan'] if args.ubsan else [])])
    print(f'J.10.6.9.6 available check groups: {sum(results)}/{len(results)} passed', flush=True)
    print('Native Windows SDK linking, controls/DPI/GDI and live audio/game acceptance still require a native build/run.', flush=True)
    return 0 if all(results) else 1

if __name__ == '__main__': raise SystemExit(main())
