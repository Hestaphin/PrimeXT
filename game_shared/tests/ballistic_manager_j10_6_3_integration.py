#!/usr/bin/env python3
"""Actual manager/flight/impact methods with deterministic engine services.

Damage dispatch invokes a real reentrant Add through the target adapter. Engine
tracing and presentation are adapted; live timing/save files remain unverified.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from pose_j10_6_2_integration import function

ROOT=Path(__file__).resolve().parents[2]

def main():
    source=(ROOT/'server/modular_ballistics.cpp').read_text()
    code=source[source.index('Vector GamePoint('):source.index('// These are gameplay-oriented')]
    shot=function(source,'struct Shot {')+';\n'
    for signature in ['bool BuildCurve','modular::BallisticState At','void Impact',
                      'bool AdvanceShot','void CModularBallisticManager::Spawn',
                      'bool CModularBallisticManager::Add','void CModularBallisticManager::Update',
                      'CModularBallisticManager::~CModularBallisticManager',
                      'int CModularBallisticManager::Save','int CModularBallisticManager::Restore']:
        code+=function(source,signature)+'\n'
    disk=function(source,'struct ShotDisk {')+';'
    declaration=function(source,'class CModularBallisticManager :')+';'
    declaration=declaration.replace('private:','public:')
    disk_version=next(line for line in source.splitlines() if line.startswith('constexpr int kShotDiskVersion'))
    template=(ROOT/'game_shared/tests/fixtures/j10_6_3/ballistic_manager_adapter.cpp.in').read_text()
    with tempfile.TemporaryDirectory(prefix='j1063_manager_') as tmp:
        tmp=Path(tmp);unit=tmp/'manager.cpp';binary=tmp/'manager'
        unit.write_text(template.replace('@SHOT_DECLARATION@',shot).replace('@PRODUCTION@',code).replace('@DISK_DECLARATION@',disk).replace('@MANAGER_DECLARATION@',declaration).replace('@DISK_VERSION@',disk_version))
        args=[os.environ.get('CXX','c++'),'-std=c++17','-O0','-g',
            '-I'+str(ROOT/'game_shared'),'-I'+str(ROOT/'server'),str(unit),str(ROOT/'game_shared/modular_weapons.cpp'),
            *[str(ROOT/'game_shared'/s) for s in ['ballistic_flight.cpp','ballistic_penetration.cpp','ballistic_path_cache.cpp']],
            '-o',str(binary)]
        if os.environ.get('J1063_UBSAN'):
            args[1:1]=['-fsanitize=undefined','-fno-sanitize-recover=all']
        for command in [args,[str(binary),str(ROOT/'documentation/examples/modular-format3')]]:
            result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
            if result.returncode:raise RuntimeError((result.stdout+result.stderr)[-8000:])
            if result.stdout.strip():print(result.stdout.strip())

if __name__=='__main__':main()
