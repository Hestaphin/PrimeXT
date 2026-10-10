#!/usr/bin/env python3
"""J.10.6.7 production regressions, client syntax and inherited acceptance.

SDK/Windows linking and live OpenGL maps are separate checks.
"""
from pathlib import Path
import argparse
import os
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
TESTS=ROOT/'game_shared/tests'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan',action='store_true')
    parser.add_argument('--skip-inherited',action='store_true')
    args=parser.parse_args();flags=['--ubsan'] if args.ubsan else []
    checks=[(name,[sys.executable,str(TESTS/name),*flags]) for name in [
        'render_j10_6_7_integration.py','player_model_j10_6_7_integration.py','debug_draw_j10_6_7_integration.py']]
    if sys.platform.startswith('linux'):
        command=[os.environ.get('CXX','c++'),'-std=c++17','-fsyntax-only','-fno-exceptions',
            '-Wno-invalid offsetof'.replace(' ','-'),'-Wno-write-strings','-D_LINUX','-DLINUX',
            '-DCLIENT_DLL','-DCLIENT_WEAPONS','-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp',
            '-D_snprintf=snprintf','-D_vsnprintf=vsnprintf',
            *['-I'+str(ROOT/p) for p in ['client','client/render','client/visualizer','client/events','client/ui',
                'common','engine','game_shared','public']]]
        checks += [('real '+file+' syntax',command+[file]) for file in [
            'client/render/gl_rmain.cpp','client/render/gl_export.cpp']]
    if not args.skip_inherited:
        checks.append(('inherited J.10.6.6 portable/method/source/syntax acceptance',
            [sys.executable,str(TESTS/'verify_j10_6_5.py'),*flags]))
    passed=0
    for name,command in checks:
        result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
        output=(result.stdout+result.stderr).strip()
        if output:print(output[-16000:])
        print(('PASS ' if result.returncode==0 else 'FAIL ')+name,flush=True)
        passed+=result.returncode==0
    print(f'J.10.6.7 check groups: {passed}/{len(checks)} passed',flush=True)
    print('Full PhysX/Windows linking and live OpenGL rendering are not certified.',flush=True)
    return 0 if passed==len(checks) else 1

if __name__=='__main__':raise SystemExit(main())
