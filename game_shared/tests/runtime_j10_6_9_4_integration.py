"""Real context class and all modular methods, with explicit engine adapters."""
from pathlib import Path
import os
import subprocess
import tempfile
from pose_j10_6_2_integration import function

ROOT=Path(__file__).resolve().parents[2]

def main():
    shared=(ROOT/'game_shared/weapons/modular.cpp').read_text()
    # Keep all implementation bytes below the includes, including both DLL branches.
    production=shared[shared.index('namespace {'):]
    header=(ROOT/'game_shared/weapons/modular.h').read_text()
    declaration=function(header,'class CModularWeaponContext final')+';'
    declaration=declaration.replace('private:','public:')
    base=(ROOT/'game_shared/weapon_context.cpp').read_text()
    methods='\n'.join(function(base,sig) for sig in ['bool CBaseWeaponContext :: DefaultDeploy','BOOL CBaseWeaponContext :: DefaultReload'])
    fixture=(ROOT/'game_shared/tests/fixtures/j10_6_9_4/runtime_adapter.cpp.in').read_text()
    npc=function((ROOT/'server/modular_npc.cpp').read_text(),'int FireModularNpcWeapon')
    with tempfile.TemporaryDirectory(prefix='j10694_runtime_') as directory:
        temporary=Path(directory);unit=temporary/'runtime.cpp'
        unit.write_text(fixture.replace('@CONTEXT_DECLARATION@',declaration).replace('@BASE_METHODS@',methods).replace('@PRODUCTION@',production).replace('@NPC_PRODUCTION@',npc))
        for side,defines in [('server',[]),('client',['-DCLIENT_DLL'])]:
            binary=temporary/side
            flags=['-fsanitize=undefined','-fno-sanitize-recover=all'] if os.environ.get('J10694_UBSAN') else []
            commands=[[os.environ.get('CXX','c++'),'-std=c++17','-O0','-g',*flags,*defines,
                '-I'+str(ROOT/'game_shared'),'-I'+str(ROOT/'server'),str(unit),
                str(ROOT/'game_shared/modular_weapons.cpp'),'-o',str(binary)],
                [str(binary),str(ROOT/'documentation/examples/modular-format3')]]
            for command in commands:
                result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
                if result.returncode:raise RuntimeError((result.stdout+result.stderr)[-7000:])
                if result.stdout.strip():print(side+': '+result.stdout.strip(),flush=True)

if __name__=='__main__':main()
