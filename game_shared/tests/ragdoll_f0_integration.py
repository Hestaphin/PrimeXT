#!/usr/bin/env python3
"""Compile actual F0 recording methods with read-only getter and UI adapters.

This is not a native PhysX SDK compile, physics simulation or pxmv GUI test.
"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / 'game_shared/tests/fixtures/j10_6_10'

def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    # These selected methods contain no brace-bearing string literals.
    depth, end = 1, brace + 1
    while depth:
        if text[end] == '{': depth += 1
        if text[end] == '}': depth -= 1
        end += 1
    return text[start:end]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan', action='store_true')
    args = parser.parse_args()
    runtime = (ROOT / 'server/physx/physics_asset.cpp').read_text()
    editor = (ROOT / 'utils/pxmv/PhysicsAssetEditor.cpp').read_text()
    selected = [function(runtime, 'void PhysicsAssetInstance::RecordDiagnostics')]
    selected += [function(editor, 'void PhysicsAssetEditor::' + name) for name in
                 ('ToggleDiagnostics', 'BeginDiagnosticCapture', 'RecordPreviewDiagnostics', 'SaveDiagnostics')]
    with tempfile.TemporaryDirectory(prefix='ragdoll_f0_methods_') as directory:
        directory = Path(directory)
        unit = directory / 'methods.cpp'
        unit.write_text((FIXTURE / 'diagnostic_methods.cpp.in').read_text().replace('@PRODUCTION@', '\n\n'.join(selected)))
        flags = ['-std=c++17', '-O0', '-g', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                 '-I' + str(FIXTURE), '-I' + str(ROOT / 'game_shared'), '-I' + str(ROOT / 'server/physx')]
        if args.ubsan: flags += ['-fsanitize=undefined', '-fno-sanitize-recover=all']
        for enabled in (True, False):
            binary = directory / ('methods_on' if enabled else 'methods_off')
            command = [os.environ.get('CXX', 'c++'), *flags,
                       *(['-DPHYSICS_ASSET_EDITOR_PHYSX=1'] if enabled else []), str(unit),
                       str(ROOT / 'game_shared/physics_asset_format.cpp'), '-o', str(binary)]
            subprocess.run(command, cwd=ROOT, check=True)
            testdir = directory / ('on' if enabled else 'off')
            testdir.mkdir()
            subprocess.run([str(binary), str(testdir)], cwd=ROOT, check=True)

if __name__ == '__main__': main()
