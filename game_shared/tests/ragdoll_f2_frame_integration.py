#!/usr/bin/env python3
"""Compile/execute actual runtime/editor D6 setters and shared frame adapter.
The explicit SDK facade only provides matrix/quat constructors and setters.
"""
from pathlib import Path
import argparse, os, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[2]
TESTS=ROOT/'game_shared/tests'
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--ubsan',action='store_true');args=parser.parse_args()
    for path in ['server/physx/physics_asset.cpp','utils/pxmv/PhysicsAssetEditor.cpp']: assert 'physicsasset::CreateAssetJoint(' in (ROOT/path).read_text()
    chunks=['#include "physics_asset_construction.h"']
    for name in ['RuntimeSettings','PreviewSettings']:
        chunks.append('void '+name+'(PxD6Joint *joint,const physicsasset::JointDef &def) {physicsasset::AssetDef a;if(!physicsasset::ConfigureD6Joint(*joint,a,def,1,false)) std::abort();}')
    fixture=TESTS/'fixtures/j10_6_10_2/frame_setters.cpp.in'
    source=fixture.read_text().replace('@PRODUCTION@','\n'.join(chunks))
    # Preserve include resolution from the fixture directory.
    source=source.replace('"../../../../server','"'+str(ROOT/'server'))
    with tempfile.TemporaryDirectory(prefix='f2_frame_') as tmp:
        tmp=Path(tmp);unit=tmp/'frame.cpp';unit.write_text(source);binary=tmp/'frame'
        flags=['-std=c++17','-O0','-Wall','-Wextra','-Werror','-Wpedantic','-fno-exceptions']
        if args.ubsan:flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
        run=subprocess.run([os.environ.get('CXX','c++'),*flags,'-I'+str(TESTS/'fixtures/j10_6_10_3'),'-I'+str(ROOT/'game_shared'),'-I'+str(ROOT/'server/physx'),str(unit),str(ROOT/'game_shared/physics_asset_format.cpp'),'-o',str(binary)],text=True,capture_output=True)
        if run.returncode:print(run.stdout+run.stderr);return run.returncode
        run=subprocess.run([str(binary)],text=True,capture_output=True);print(run.stdout+run.stderr);return run.returncode
if __name__=='__main__':raise SystemExit(main())
