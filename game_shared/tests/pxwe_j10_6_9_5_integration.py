#!/usr/bin/env python3
"""Run unchanged native pxwe controller methods with explicit Win32 adapters.

These checks use the real C++ backend but do not certify the Windows GUI.
"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
from pose_j10_6_2_integration import function

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan', action='store_true')
    args = parser.parse_args()
    source = (ROOT/'tools/weapon_editor/pxwe_win32.cpp').read_text()
    signatures = ['HWND ControlWindow(', 'void Move(', 'int ActiveTab() const', 'void Layout()', 'void ShowPage()', 'void SyncCameraControls()', 'void RenameSelectedAsset()',
                  'void RefreshProperties()', 'void EditProperty(', 'void ActivateProperty(',
                  'void BrowseProperty()', 'void CommitEditor()', 'void NewAsset(', 'LRESULT Notify(']
    methods = '\n\n'.join(function(source, s) for s in signatures)
    enum = function(source, 'enum Control') + ';'
    fixture = (ROOT/'game_shared/tests/fixtures/j10_6_9_5/pxwe_ui_adapter.cpp.in').read_text()
    unit = fixture.replace('@PRODUCTION@', methods).replace('@ENUM@', enum)
    with tempfile.TemporaryDirectory(prefix='pxwe_ui_') as directory:
        directory = Path(directory); cpp = directory/'ui.cpp'; binary = directory/'ui'
        environment = os.environ.copy(); environment['TMPDIR'] = str(directory)
        cpp.write_text(unit)
        compiler = os.environ.get('CXX', 'c++')
        command = [compiler, '-std=c++17', '-O0', '-g', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                   '-I'+str(ROOT/'tools/weapon_editor'), '-I'+str(ROOT/'game_shared'), '-I'+str(ROOT/'game_shared/tests')]
        if args.ubsan: command += ['-fsanitize=undefined', '-fno-sanitize-recover=all']
        command += [str(cpp), str(ROOT/'tools/weapon_editor/pxwe_properties.cpp'),
                    str(ROOT/'tools/weapon_editor/pxwe_project.cpp'), str(ROOT/'tools/weapon_editor/pxwe_preview.cpp'), str(ROOT/'game_shared/modular_weapons.cpp'),
                    str(ROOT/'game_shared/ballistic_flight.cpp'), '-o', str(binary)]
        build = subprocess.run(command, capture_output=True, text=True, env=environment)
        if build.returncode: raise RuntimeError(build.stdout+build.stderr)
        run = subprocess.run([str(binary)], capture_output=True, text=True, env=environment)
        print(run.stdout+run.stderr, end='', flush=True)
        return run.returncode

if __name__ == '__main__': raise SystemExit(main())
