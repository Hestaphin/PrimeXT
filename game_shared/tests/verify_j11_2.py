#!/usr/bin/env python3
"""J.11.2 portable acceptance suite. Does not replace live engine/network testing."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
T=ROOT/'game_shared/tests'
CXX=os.environ.get('CXX','c++')

def run(label, cmd):
    p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
    print(('PASS' if p.returncode==0 else 'FAIL')+' '+label,flush=True)
    if p.returncode:
        print((p.stdout+p.stderr)[-10000:],flush=True)
    elif p.stdout.strip():print(p.stdout.strip()[-1200:],flush=True)
    return p.returncode==0

def main():
    passed=total=0
    with tempfile.TemporaryDirectory(prefix='j112_') as tmp:
        for name,sources in [
            ('modular_j11_1_schema_tests',['game_shared/modular_weapons.cpp']),
            ('modular_j11_2_damage_tests',[])]:
            binary=str(Path(tmp)/name)
            if run('compile '+name,[CXX,'-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'game_shared'),
                str(T/(name+'.cpp')),*[str(ROOT/s) for s in sources],'-o',binary]):
                total+=1;passed+=run(name,[binary])
            else:total+=1
        for label,script in [
            ('J.11.2 loaded-cartridge client/server adapter','runtime_j11_2_integration.py'),
            ('J.10.6.9.4 player/NPC inherited adapter','runtime_j10_6_9_4_integration.py'),
            ('J.10.6.3 ballistic save/restore inherited adapter','ballistic_manager_j10_6_3_integration.py'),
            ('J.11.2 actual ballistic v4 impact/save/restore adapter','ballistic_manager_j11_2_integration.py')]:
            total+=1;passed+=run(label,[sys.executable,str(T/script)])
    print(f'J.11.2 acceptance: {passed}/{total} passed',flush=True)
    return 0 if passed==total else 1
if __name__=='__main__':sys.exit(main())
