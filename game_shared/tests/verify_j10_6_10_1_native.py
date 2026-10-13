#!/usr/bin/env python3
"""Configure/link real server, client and pxmv with PhysX using installed SDKs.

Run this on the native build machine. No SDK facade is used. A completed build
does not certify GUI interaction, solver stability, game behavior or FPS.
"""
from pathlib import Path
import argparse
import json
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir',type=Path,required=True,help='Separate native build output directory')
    parser.add_argument('--generator',help='For example, Visual Studio 17 2022')
    parser.add_argument('--arch',help='CMake generator platform, such as Win32 or x64')
    parser.add_argument('--toolchain',type=Path,help='Installed vcpkg toolchain file')
    parser.add_argument('--triplet',help='Optional vcpkg target triplet matching the game architecture')
    parser.add_argument('--prefix',action='append',default=[],help='Dependency prefix; may be repeated')
    parser.add_argument('--config',default='RelWithDebInfo')
    parser.add_argument('--physx',choices=['ON','OFF'],default='ON',
                        help='Repeat with OFF in a different build directory to check the disabled variant')
    parser.add_argument('--json',type=Path,required=True)
    args=parser.parse_args()
    result={'milestone':'J.10.6.10.1','physx':args.physx,'native_build':'NOT_RUN','commands':[],
            'gui_solver_game_fps':'NOT_RUN; perform ACCEPTANCE.md after a successful native build'}
    def finish(code):
        args.json.parent.mkdir(parents=True,exist_ok=True)
        args.json.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2));return code
    cmake=shutil.which('cmake')
    if not cmake:
        result.update(native_build='BLOCKED',reason='CMake executable is unavailable; no native configuration or link was attempted.')
        return finish(2)
    if args.toolchain and not args.toolchain.is_file():
        result.update(native_build='BLOCKED',reason='The supplied native toolchain file does not exist.')
        return finish(2)
    build=args.build_dir.resolve()
    if build==ROOT or ROOT in build.parents:
        parser.error('Choose a separate build directory, not a source directory.')
    configure=[cmake,'-S',str(ROOT),'-B',str(build),'-DENABLE_PHYSX='+args.physx,
               '-DBUILD_SERVER=ON','-DBUILD_CLIENT=ON','-DBUILD_UTILS=ON','-DBUILD_GAME_LAUNCHER=OFF',
               '-DCMAKE_BUILD_TYPE='+args.config]
    if args.generator:configure+=['-G',args.generator]
    if args.arch:configure+=['-A',args.arch]
    if args.toolchain:configure+=['-DCMAKE_TOOLCHAIN_FILE='+str(args.toolchain.resolve())]
    if args.triplet:configure+=['-DVCPKG_TARGET_TRIPLET='+args.triplet]
    if args.prefix:configure+=['-DCMAKE_PREFIX_PATH='+';'.join(args.prefix)]
    link=[cmake,'--build',str(build),'--config',args.config,'--target','server','client','pxmv']
    args.json.parent.mkdir(parents=True,exist_ok=True)
    for phase,command in [('configure',configure),('link',link)]:
        log=args.json.with_name(args.json.stem+'_'+phase+'.log')
        with log.open('w') as output:
            run=subprocess.run(command,cwd=ROOT,stdout=output,stderr=subprocess.STDOUT,text=True)
        result['commands'].append({'phase':phase,'argv':command,'exit_code':run.returncode,'log':str(log.resolve())})
        if run.returncode:
            result.update(native_build='FAIL',reason=phase+' failed; inspect the real build log.')
            return finish(run.returncode)
    result['native_build']='PASS'
    return finish(0)

if __name__=='__main__':raise SystemExit(main())
