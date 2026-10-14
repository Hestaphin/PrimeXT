#!/usr/bin/env python3
"""Execute actual J.10.6.1 engine-query and Prepare/Begin methods in isolation.
The adapters supply deterministic engine services/model data. This is not a
linked-engine, live-map or SDK test. Production algorithms are not rewritten.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
XASH = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT.parent / 'xash3d-fwgs-master'
CC = os.environ.get('CC','cc')
CXX = os.environ.get('CXX','c++')

def run(args):
    result = subprocess.run(args,capture_output=True,text=True)
    if result.returncode:
        print(result.stdout+result.stderr)
        raise SystemExit(result.returncode)
    if result.stdout:
        print(result.stdout.strip())

def main():
    source = (ROOT/'server/modular_ballistics.cpp').read_text()
    coordinates = source[source.index('Vector GamePoint('):source.index('modular::BallisticVec3 Mix(')]
    materials = source[source.index('struct PenetrationMaterial '):source.index('struct Shot {')]
    methods = source[source.index('struct PenetrationCandidate {'):source.index('// Invoked once per shot')]
    with tempfile.TemporaryDirectory(prefix='j1061_methods_',dir=ROOT) as tmp:
        tmp=Path(tmp)
        c=ROOT/'game_shared/tests/fixtures/j10_6_1/engine_adapter.c'
        template=(c.parent/'game_adapter.cpp.in').read_text()
        cpp=tmp/'game_adapter.cpp'
        cpp.write_text(template.replace('// @PRODUCTION_CODE@',coordinates+materials+methods))
        obj=tmp/'engine_adapter.o';binary=tmp/'runtime_cases'
        run([CC,'-std=gnu99','-Wall','-Wextra','-Werror',
             '-I'+str(XASH/'engine'),'-I'+str(XASH/'engine/common'),
             '-I'+str(XASH/'engine/server'),'-c',str(c),'-o',str(obj)])
        run([CXX,'-std=c++17','-Wno-unknown-pragmas',
             '-I'+str(ROOT/'common'),'-I'+str(ROOT/'public'),
             '-I'+str(ROOT/'engine'),'-I'+str(ROOT/'game_shared'),
             str(cpp),str(obj),str(ROOT/'game_shared/ballistic_flight.cpp'),
             str(ROOT/'game_shared/ballistic_penetration.cpp'),'-o',str(binary)])
        run([str(binary)])

if __name__=='__main__':
    main()
