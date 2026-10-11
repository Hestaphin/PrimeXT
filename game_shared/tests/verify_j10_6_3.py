#!/usr/bin/env python3
"""J.10.6.3 portable/method acceptance. SDK builds and live maps are separate.

Run from any directory. Requires Python 3, C and C++17 compilers. --ubsan adds
undefined-behavior checks to the new production-method regressions on Linux.
"""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from verify_j10_6 import TEST_CASES

ROOT=Path(__file__).resolve().parents[2]
TESTS=ROOT/'game_shared/tests'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan',action='store_true')
    parser.add_argument('--skip-inherited',action='store_true',help='omit the already verified J.10.6.2 groups')
    args=parser.parse_args()
    passed=total=0
    env=os.environ.copy()
    if args.ubsan:env['J1063_UBSAN']='1'
    def run(command,cwd=ROOT):
        result=subprocess.run(command,cwd=cwd,env=env,capture_output=True,text=True)
        if result.returncode:raise RuntimeError((result.stdout+result.stderr)[-8000:])
        if result.stdout.strip():print(result.stdout.strip()[-6000:])
        if 'unittest' in command and result.stderr.strip():print(result.stderr.strip()[-6000:])
    def check(name,action):
        nonlocal passed,total
        total+=1
        try:action();passed+=1;print('PASS '+name,flush=True)
        except (OSError,RuntimeError,AssertionError) as error:print('FAIL '+name+': '+str(error),flush=True)
    check('actual death/weight/impulse and shape-query production methods',
        lambda:run([sys.executable,str(TESTS/'physics_j10_6_3_integration.py')]))
    check('actual 256-shot manager and reentrant damage dispatch',
        lambda:run([sys.executable,str(TESTS/'ballistic_manager_j10_6_3_integration.py')]))
    if not args.skip_inherited:
        check('inherited CPU pose, inventory, BSP, source and syntax groups',
            lambda:run([sys.executable,str(TESTS/'verify_j10_6_2.py')]))
    compiler=os.environ.get('CXX','c++')
    with tempfile.TemporaryDirectory(prefix='j1063_acceptance_') as tmp:
        tmp=Path(tmp)
        env['TMPDIR']=str(tmp)
        includes=['-I'+str(ROOT/p) for p in ['game_shared','engine','common','public']]
        options=['-std=c++17','-O0',*includes]
        if sys.platform.startswith('linux'):
            options+=['-D_LINUX','-DLINUX','-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp',
                '-D_snprintf=snprintf','-D_vsnprintf=vsnprintf','-ffunction-sections','-fdata-sections']
        sources=['physics_asset_format.cpp','modular_weapons.cpp','ballistic_flight.cpp',
                 'ballistic_penetration.cpp','ballistic_path_cache.cpp','vector.cpp','matrix.cpp','mathlib.cpp','stringlib.cpp']
        objects=[]
        def prepare():
            for source in sources:
                obj=tmp/(source+'.o')
                run([compiler,*options,'-c',str(ROOT/'game_shared'/source),'-o',str(obj)])
                objects.append(str(obj))
        check('compile real shared parser/flight/penetration/cache objects',prepare)
        covered={name for name,_,_ in TEST_CASES}|{'ballistic_j10_6_1_tests'}
        for source in sorted(TESTS.glob('*.cpp')):
            if source.stem in covered:continue
            def portable(source=source):
                binary=tmp/source.stem
                link=['-Wl,--gc-sections'] if sys.platform.startswith('linux') else []
                run([compiler,*options,str(source),*objects,*link,'-o',str(binary)])
                command=[str(binary)]
                if source.stem=='physics_part_i9_tests':command.append(str(ROOT/'documentation/examples/interactive_car_door.phys'))
                if source.stem=='ballistic_flight_test':command.append(str(ROOT/'documentation/examples/modular-ballistics'))
                run(command,cwd=tmp)
            check('extra portable '+source.stem,portable)
        validator=tmp/'primext-weapon-validate'
        def native():
            # Exactly the two sources declared by tools/weapon_editor/CMakeLists.
            run([compiler,*options,str(ROOT/'tools/weapon_editor/native_validate.cpp'),
                 str(tmp/'modular_weapons.cpp.o'),'-o',str(validator)])
        check('link native weapon validator from declared sources',native)
        for fixture in ['documentation/examples/modular-weapon','documentation/examples/modular-ballistics','game_shared/tests/fixtures/j4']:
            check('native validator '+fixture,lambda fixture=fixture:run([str(validator),str(ROOT/fixture)]))
        def rejected():
            invalid=tmp/'invalid-pack';shutil.copytree(ROOT/'documentation/examples/modular-ballistics',invalid)
            cartridge=invalid/'scripts/cartridges/test_jhp.cartridge'
            content=cartridge.read_text();assert 'damage = 18' in content
            cartridge.write_text(content.replace('damage = 18','damage = nan'))
            result=subprocess.run([str(validator),str(invalid)],capture_output=True,text=True)
            assert result.returncode==1 and result.stderr.strip()
        check('native validator rejects invalid cartridge pack',rejected)
        env['PRIMEXT_WEAPON_VALIDATOR']=str(validator)
        check('headless weapon editor and validator discovery suite',lambda:run([
            sys.executable,'-m','unittest','discover','-s','tools/weapon_editor/tests','-v']))
    if sys.platform.startswith('linux'):
        common=[compiler,'-std=c++17','-fsyntax-only','-fno-exceptions','-Wno-invalid offsetof'.replace(' ','-'),
            '-D_LINUX','-DLINUX','-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp',
            '-D_snprintf=snprintf','-D_vsnprintf=vsnprintf']
        includes=['server','server/monsters','server/weapons','server/entities','server/gamerules','common','engine','game_shared','public']
        for source in ['server/combat.cpp','server/animating.cpp','server/physx/physx_impl.cpp','server/world.cpp']:
            check('real PhysX-disabled '+source+' syntax',lambda source=source:run(common+['-I'+p for p in includes]+[source]))
    print(f'J.10.6.3 available check groups: {passed}/{total} passed')
    print('Separate acceptance required: full Linux/Windows target links, real PhysX overlap/solver scenes, engine save/network/live maps.')
    return 0 if passed==total else 1

if __name__=='__main__':raise SystemExit(main())
