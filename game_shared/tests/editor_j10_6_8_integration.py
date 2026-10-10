#!/usr/bin/env python3
"""Run unmodified editor/toolkit methods with explicit platform/GL adapters.

Uses real mx, PhysicsAssetEditor, StudioModel, Studio and asset declarations,
plus real project math. This is not a Windows/Qt GUI or GPU certification.
"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile
from pose_j10_6_2_integration import function

TESTS = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-tree', type=Path, default=TESTS.parents[1])
    parser.add_argument('--ubsan', action='store_true')
    parser.add_argument('--record', type=Path)
    args = parser.parse_args()
    root = args.source_tree.resolve()
    editor = (root/'utils/pxmv/PhysicsAssetEditor.cpp').read_text()
    studio = (root/'utils/pxmv/studio_render.cpp').read_text()
    utils = (root/'utils/pxmv/studio_utils.cpp').read_text()
    gl = (root/'utils/pxmv/GlWindow.cpp').read_text()
    win = (root/'external/mxtk/src/win32/mxWindow.cpp').read_text()
    qt = (root/'external/mxtk/src/qt/mxWindow.cpp').read_text()
    win_methods = win[win.index('class mxWindow_i'):win.index('void\nmxWindow::setTimer')]
    qt_methods = function(qt, 'mxWindow::mxWindow')
    editor_methods = [function(editor, signature) for signature in [
        'static Vector ToVector', 'static matrix3x4 BodyLocalMatrix',
        'static void DrawUnitCircle', 'static void DrawBox',
        'static void DrawSphere', 'static void DrawCapsule',
        'PhysicsAssetEditor::PhysicsAssetEditor',
        'void PhysicsAssetEditor::BuildUI', 'int PhysicsAssetEditor::FindBone(',
        'void PhysicsAssetEditor::DrawOverlay']]
    editor_methods += [function(studio, 'StudioModel::StudioModel'),
                       function(utils, 'const matrix3x4 &StudioModel::GetToolBoneTransform')]
    if 'StudioModel::GetToolModelTransform' in studio:
        editor_methods.append(function(studio, 'const matrix3x4 &StudioModel::GetToolModelTransform'))
    idle = function(gl, 'case mxEvent::Idle:')
    groups = [
        ('win_dialog', win_methods, ['owned-dialog', 'client-bounds', 'normal-child', 'modal-dialog']),
        ('qt_dialog', qt_methods, ['owned-dialog', 'normal-child', 'modal-dialog']),
        ('editor', '\n\n'.join(editor_methods), ['layout', 'occlusion-state', 'root-transform',
            'bone-transform', 'invalid-joint', 'hidden-overlay', 'empty-overlay']),
        ('idle', idle, ['paused-simulation', 'paused-visibility', 'animation-control']),
    ]
    records = []
    with tempfile.TemporaryDirectory(prefix='j1068_editor_') as directory:
        directory = Path(directory)
        for name, production, cases in groups:
            unit = (TESTS/f'fixtures/j10_6_8/{name}_adapter.cpp.in').read_text()
            unit = unit.replace('@PRODUCTION@', production)
            source = directory/(name+'.cpp')
            source.write_text(unit)
            binary = directory/name
            includes = ['utils/pxmv', 'external/mxtk/include', 'client/render',
                        'common', 'engine', 'game_shared', 'public']
            command = [os.environ.get('CXX', 'c++'), '-std=c++17', '-O0', '-g',
                '-ffunction-sections', '-fdata-sections', '-D_LINUX', '-DLINUX',
                '-Dstricmp=strcasecmp', '-Dstrnicmp=strncasecmp',
                '-D_snprintf=snprintf', '-D_vsnprintf=vsnprintf', '-Wno-write-strings',
                '-Wno-invalid offsetof'.replace(' ', '-'),
                *['-I'+str(root/p) for p in includes], str(source)]
            if args.ubsan:
                command += ['-fsanitize=undefined', '-fno-sanitize-recover=all']
            if name == 'editor':
                command += [str(root/'game_shared'/p) for p in [
                    'vector.cpp', 'matrix.cpp', 'mathlib.cpp', 'stringlib.cpp',
                    'bone_setup.cpp', 'ikcontext.cpp', 'procbones.cpp', 'jigglebones.cpp',
                    'physics_asset_format.cpp']]
            command += ['-Wl,--gc-sections', '-o', str(binary)]
            build = subprocess.run(command, cwd=root, capture_output=True, text=True)
            if build.returncode:
                raise RuntimeError((build.stdout+build.stderr)[-8000:])
            for case in cases:
                result = subprocess.run([str(binary), case], cwd=directory, capture_output=True, text=True)
                output = (result.stdout+result.stderr).strip()
                records.append({'group': name, 'case': case, 'returncode': result.returncode, 'output': output})
                print(('PASS ' if result.returncode == 0 else 'FAIL ')+name+'/'+case+': '+output[-1200:], flush=True)
    if args.record:
        args.record.parent.mkdir(parents=True, exist_ok=True)
        args.record.write_text(json.dumps(records, indent=2)+'\n')
    passed = sum(r['returncode'] == 0 for r in records)
    print(f'J.10.6.8 editor method cases: {passed}/{len(records)} passed', flush=True)
    return 0 if passed == len(records) else 1


if __name__ == '__main__':
    raise SystemExit(main())
