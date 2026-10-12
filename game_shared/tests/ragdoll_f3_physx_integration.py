#!/usr/bin/env python3
"""Execute production shared constructors, shaders and runtime lifetime methods.
The recording facade does not simulate PhysX. Native linking is separate.
"""
from pathlib import Path
import argparse, os, subprocess, tempfile
from ragdoll_f0_integration import function
ROOT=Path(__file__).resolve().parents[2]
TESTS=ROOT/'game_shared/tests'
def shader(source,name):
    start=source.index('sceneDesc.filterShader = []')
    start=source.index('(',start)
    end=source.index('\n',source.index('};',start))
    return 'PxFilterFlags '+name+source[start:end].replace(' -> PxFilterFlags','').rstrip().removesuffix(';')
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ubsan',action='store_true');a=p.parse_args()
    runtime=(ROOT/'server/physx/physics_asset.cpp').read_text()
    chunks=[function(runtime,s) for s in ['void PhysicsAssetInstance::DestroyActors','void PhysicsAssetInstance::SetBodyKinematicState']]
    chunks+=[shader((ROOT/'server/physx/physx_impl.cpp').read_text(),'RuntimeShader'),shader((ROOT/'utils/pxmv/PhysicsAssetEditor.cpp').read_text(),'PreviewShader')]
    text=(TESTS/'fixtures/j10_6_10_3/construction_tests.cpp.in').read_text().replace('@PRODUCTION@','\n'.join(chunks))
    with tempfile.TemporaryDirectory(prefix='f3_sdk_') as tmp:
        tmp=Path(tmp);unit=tmp/'sdk.cpp';unit.write_text(text);binary=tmp/'sdk'
        flags=['-std=c++17','-O0','-g','-fno-exceptions','-Wall','-Wextra','-Werror','-Wpedantic','-Wno-unused-parameter','-Wno-misleading-indentation','-pthread']
        if a.ubsan:flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
        cmd=[os.environ.get('CXX','c++'),*flags,*['-I'+str(ROOT/x) for x in ['game_shared/tests/fixtures/j10_6_10_3','game_shared','server/physx']],str(unit),str(ROOT/'game_shared/physics_asset_format.cpp'),str(ROOT/'server/physx/collision_filter_data.cpp'),'-o',str(binary)]
        r=subprocess.run(cmd,text=True,capture_output=True)
        if r.returncode:print(r.stdout+r.stderr);return r.returncode
        r=subprocess.run([str(binary)],text=True,capture_output=True);print(r.stdout+r.stderr);return r.returncode
if __name__=='__main__':raise SystemExit(main())
