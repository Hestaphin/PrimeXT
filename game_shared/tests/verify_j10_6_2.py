#!/usr/bin/env python3
"""J.10.6.2 actual-method regressions and changed real translation-unit checks.

Requires Python 3/C++17. Linux configuration checks use the real project headers,
PhysX disabled and client weapon prediction enabled. Full linking/live maps are
separate acceptance checks.
"""
from pathlib import Path
import os
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
TESTS=ROOT/'game_shared/tests'

def main():
    passed=total=0
    def check(name,args):
        nonlocal passed,total
        total+=1
        result=subprocess.run(args,cwd=ROOT,capture_output=True,text=True)
        output=(result.stdout+result.stderr).strip()
        if output: print(output[-7500:])
        print(('PASS ' if result.returncode==0 else 'FAIL ')+name,flush=True)
        if result.returncode==0: passed+=1
    check('actual shared CPU pose / physical packet / draw methods',
        [sys.executable,str(TESTS/'pose_j10_6_2_integration.py')])
    check('actual typed pickup / box / restore / respawn methods',
        [sys.executable,str(TESTS/'inventory_j10_6_2_integration.py')])
    check('inherited J.10.6.1 groups, old suite and current source wiring',
        [sys.executable,str(TESTS/'verify_j10_6_1.py')])
    if sys.platform.startswith('linux'):
        common=[os.environ.get('CXX','c++'),'-std=c++17','-fsyntax-only','-fno-exceptions',
            '-Wno-invalid offsetof'.replace(' ','-'),'-Wno-write-strings',
            '-D_LINUX','-DLINUX','-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp',
            '-D_snprintf=snprintf','-D_vsnprintf=vsnprintf']
        files=['server/player.cpp','server/weapons.cpp','server/weapons/weapon_modular.cpp',
            'server/gamerules/gamerules.cpp','server/gamerules/multiplay_gamerules.cpp',
            'client/render/gl_studio_draw.cpp','client/render/gl_studio_init.cpp',
            'client/utils.cpp','client/hud_msg.cpp']
        for file in files:
            server=file.startswith('server/')
            includes=['server','server/monsters','server/weapons','server/entities','server/gamerules'] if server else ['client','client/render','client/visualizer','client/events']
            defs=[] if server else ['-DCLIENT_DLL','-DCLIENT_WEAPONS']
            check('real '+file+' syntax',common+defs+
                ['-I'+p for p in includes+['common','engine','game_shared','public']]+[file])
    else: print('SKIP Linux translation-unit configuration on this platform')
    print(f'J.10.6.2 check groups: {passed}/{total} passed')
    print('Engine/SDK linking, native save files and live maps are not certified.')
    return 0 if passed==total else 1

if __name__=='__main__': raise SystemExit(main())
