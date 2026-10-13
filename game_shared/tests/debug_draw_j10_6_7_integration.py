#!/usr/bin/env python3
"""Execute production DebugDraw against explicit PhysX/scene service adapters.

Checks actor filtering and toggling; this is not an SDK build or simulation.
"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile
from pose_j10_6_2_integration import function

TESTS=Path(__file__).resolve().parent

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-tree',type=Path,default=TESTS.parents[1])
    parser.add_argument('--ubsan',action='store_true')
    parser.add_argument('--record',type=Path)
    args=parser.parse_args();root=args.source_tree.resolve()
    production=function((root/'server/physx/physx_impl.cpp').read_text(),'void CPhysicPhysX :: DebugDraw')
    template=(TESTS/'fixtures/j10_6_7/debug_draw_adapter.cpp.in').read_text()
    with tempfile.TemporaryDirectory(prefix='j1067_debug_') as directory:
        directory=Path(directory);source=directory/'debug.cpp';binary=directory/'debug'
        source.write_text(template.replace('@PRODUCTION@',production))
        sanitizers=['-fsanitize=undefined','-fno-sanitize-recover=all'] if args.ubsan else []
        build=subprocess.run([os.environ.get('CXX','c++'),*sanitizers,'-std=c++17',
            '-O0','-g',str(source),'-o',str(binary)],capture_output=True,text=True)
        if build.returncode:raise RuntimeError((build.stdout+build.stderr)[-6000:])
        result=subprocess.run([str(binary)],capture_output=True,text=True,cwd=directory)
        record={'case':'ragdoll-only/general/off toggles','returncode':result.returncode,
            'output':(result.stdout+result.stderr).strip()}
        print(('PASS ' if result.returncode==0 else 'FAIL ')+record['case']+': '+record['output'],flush=True)
        if args.record:
            args.record.parent.mkdir(parents=True,exist_ok=True)
            args.record.write_text(json.dumps(record,indent=2)+'\n')
        return result.returncode

if __name__=='__main__':raise SystemExit(main())
