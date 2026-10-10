#!/usr/bin/env python3
"""J.10.6.1 portable/method checks; real SDK builds and live maps remain required."""
from pathlib import Path
import argparse
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT/'game_shared/tests'
CC = os.environ.get('CC','cc')
CXX = os.environ.get('CXX','c++')
EXECUTABLES = ["android/app/run-python","android/gradlew","engine/platform/dos/ld.sh","engine/platform/dos/objcopy.sh","engine/platform/irix/xash-exec","scripts/cirrus/build_freebsd.sh","scripts/configure-ninja.py","scripts/flatpak/run.sh","scripts/gha/build_android.sh","scripts/gha/build_apple.sh","scripts/gha/build_ios.sh","scripts/gha/build_linux-e2k.sh","scripts/gha/build_linux.sh","scripts/gha/build_motomagx.sh","scripts/gha/build_win32.sh","scripts/gha/deps_android.sh","scripts/gha/deps_apple.sh","scripts/gha/deps_linux-e2k.sh","scripts/gha/deps_linux.sh","scripts/gha/deps_motomagx.sh","scripts/gha/deps_win32.sh","scripts/gha/make_release.sh","scripts/ios/buildallmods.sh","scripts/ios/buildcs16.sh","scripts/ios/buildhlsdk.sh","scripts/ios/createipa.sh","scripts/sailfish/build.sh","scripts/sailfish/deploy.sh","scripts/waifulib/owcc.py","utils/bshift-swap/bshift-swap.py","waf"]

def run(args,cwd=ROOT,quiet=False):
    result=subprocess.run(args,cwd=cwd,capture_output=True,text=True)
    if result.returncode or not quiet:
        output=(result.stdout+result.stderr).strip()
        if output:
            print(output[-4500:])
    if result.returncode:
        raise RuntimeError('command failed: '+str(args[0]))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xash-root',type=Path,default=ROOT.parent/'xash3d-fwgs-master')
    args=parser.parse_args()
    xash=args.xash_root.resolve()
    passed=total=0
    def check(name,action):
        nonlocal passed,total
        total+=1
        try:
            action()
            print('PASS '+name);passed+=1
        except (OSError,RuntimeError,AssertionError) as error:
            print('FAIL '+name+': '+str(error))
    def headers():
        assert (ROOT/'engine/world_ballistic_query.h').read_bytes()==(xash/'engine/world_ballistic_query.h').read_bytes()
    def modes():
        if os.name=='nt':
            print('SKIP POSIX execute-bit/direct-launch check on Windows')
            return
        missing=[p for p in EXECUTABLES if not (xash/p).stat().st_mode&0o111]
        assert not missing,repr(missing)
        run([str(xash/'waf'),'--help'],cwd=xash,quiet=True)
    check('identical C/C++ native query contract',headers)
    if os.name!='nt':
        check('31 executable modes and direct waf launch',modes)
    else:
        print('SKIP POSIX execute-bit/direct-launch group on Windows')
    with tempfile.TemporaryDirectory(prefix='j1061_verify_',dir=ROOT) as tmp:
        tmp=Path(tmp)
        def interval():
            binary=tmp/'interval'
            run([CC,'-std=c99','-Wall','-Wextra','-Werror',
                str(xash/'engine/tests/world_solid_interval_tests.c'),'-lm','-o',str(binary)],quiet=True)
            run([str(binary)])
        def policy():
            binary=tmp/'policy'
            run([CXX,'-std=c++17','-Wall','-Wextra','-Werror',
                '-I'+str(ROOT/'engine'),'-I'+str(ROOT/'game_shared'),
                str(TESTS/'ballistic_j10_6_1_tests.cpp'),'-o',str(binary)],quiet=True)
            run([str(binary)])
        check('production exact BSP interval traversal',interval)
        check('game-side API/geometry/material policy',policy)
        check('actual engine query and Prepare/Begin methods with explicit service adapters',
            lambda:run([sys.executable,str(TESTS/'ballistic_j10_6_1_integration.py'),str(xash)]))
    check('existing J.10.6 portable/source suite',
        lambda:run([sys.executable,str(TESTS/'verify_j10_6.py')]))
    if sys.platform.startswith('linux'):
        check('real server/modular_ballistics.cpp syntax (PhysX disabled)',lambda:run([
            CXX,'-std=c++17','-fsyntax-only','-fno-exceptions','-Wno-invalid-offsetof',
            '-D_LINUX','-DLINUX','-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp',
            '-D_snprintf=snprintf','-D_vsnprintf=vsnprintf',
            '-Iserver','-Iserver/monsters','-Iserver/weapons','-Iserver/entities',
            '-Iserver/gamerules','-Icommon','-Iengine','-Igame_shared','-Ipublic',
            'server/modular_ballistics.cpp'],quiet=True))
    else:
        print('SKIP Linux server syntax configuration on this platform')
    print(f'J.10.6.1 check groups: {passed}/{total} passed')
    print('SDK linking, Windows build, live maps and hidden BSP layers are not certified.')
    return 0 if passed==total else 1

if __name__=='__main__':
    raise SystemExit(main())
