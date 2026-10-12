#!/usr/bin/env python3
"""Editor method regressions, full edited pxmv units, and inherited acceptance.

GL declarations come from the real client API through a temporary alias adapter.
No native Windows/Qt, PhysX SDK linking or live OpenGL certification is implied.
"""
from pathlib import Path
import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT/'game_shared/tests'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan', action='store_true')
    parser.add_argument('--skip-inherited', action='store_true')
    args = parser.parse_args()
    flags = ['--ubsan'] if args.ubsan else []
    checks = [('editor method regressions', [sys.executable, str(TESTS/'editor_j10_6_8_integration.py'), *flags])]
    with tempfile.TemporaryDirectory(prefix='j1068_gl_declarations_') as directory:
        bridge = Path(directory)/'GL'
        bridge.mkdir()
        export = (ROOT/'client/render/gl_export.h').read_text()
        names = sorted(set(re.findall(r'\b(pgl\w+)\b', export)))
        (bridge/'gl.h').write_text('#pragma once\n#include "gl_export.h"\n'+
            ''.join('#define '+name[1:]+' '+name+'\n' for name in names))
        if sys.platform.startswith('linux'):
            includes = ['utils/pxmv', 'external/mxtk/include', 'external/mxtk/include/mx',
                'game_shared', 'engine', 'common', 'public', 'utils/common',
                'utils/common/imagelib', 'client/render']
            command = [os.environ.get('CXX', 'c++'), '-std=c++17', '-fsyntax-only',
                '-fno-exceptions', '-Wno-write-strings', '-Wno-invalid offsetof'.replace(' ', '-'),
                '-D_LINUX', '-DLINUX', '-Dstricmp=strcasecmp', '-Dstrnicmp=strncasecmp',
                '-D_snprintf=snprintf', '-D_vsnprintf=vsnprintf', '-I'+str(bridge.parent),
                *['-I'+str(ROOT/p) for p in includes]]
            checks += [(name+' complete syntax (GL declarations adapted; PhysX off)',
                command+[str(ROOT/'utils/pxmv'/name)]) for name in [
                    'PhysicsAssetEditor.cpp', 'RagdollTemplateDialog.cpp', 'PhysicsSettingsDialog.cpp', 'studio_render.cpp', 'studio_utils.cpp', 'GlWindow.cpp']]
        if not args.skip_inherited:
            checks.append(('inherited J.10.6.7 acceptance',
                [sys.executable, str(TESTS/'verify_j10_6_7.py'), *flags]))
        passed = 0
        for name, command in checks:
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
            output = (result.stdout+result.stderr).strip()
            if output:
                print(output[-22000:])
            print(('PASS ' if result.returncode == 0 else 'FAIL ')+name, flush=True)
            passed += result.returncode == 0
    print(f'J.10.6.8 check groups: {passed}/{len(checks)} passed', flush=True)
    print('Native Windows/Qt, PhysX SDK and live rendering require a separate run.', flush=True)
    return 0 if passed == len(checks) else 1


if __name__ == '__main__':
    raise SystemExit(main())
