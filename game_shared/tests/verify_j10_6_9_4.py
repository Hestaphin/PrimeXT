#!/usr/bin/env python3
"""J.10.6.9.4 step-5 runtime checks, based on corrected J.10.6.9.3.

Real loaders, context/manager/HUD methods and complete translation units are
checked. Engine services are adapted; full links and live maps remain separate.
"""
from pathlib import Path
import argparse
import os
import re
import subprocess
import struct
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
TESTS=ROOT/'game_shared/tests'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan',action='store_true')
    parser.add_argument('--skip-inherited',action='store_true')
    args=parser.parse_args()
    results=[]
    def check(label,command,environment):
        result=subprocess.run(command,cwd=ROOT,env=environment,capture_output=True,text=True)
        output=(result.stdout+result.stderr).strip()
        if output:print(output[-30000:],flush=True)
        results.append(result.returncode==0)
        print(('PASS ' if results[-1] else 'FAIL ')+label,flush=True)
        return result
    with tempfile.TemporaryDirectory(prefix='j10694_verify_',dir=ROOT) as directory:
        temporary=Path(directory);environment=os.environ.copy();environment['TMPDIR']=str(temporary)
        if args.ubsan:
            environment['J10694_UBSAN']='1';environment['J1063_UBSAN']='1'
        compiler=os.environ.get('CXX','c++')
        sanitizer=['-fsanitize=undefined','-fno-sanitize-recover=all'] if args.ubsan else []
        outputs=[]
        for side,defines in [('client',['-DCLIENT_DLL','-DCLIENT_WEAPONS']),('server',[])]:
            binary=temporary/('assets-'+side)
            built=check(side+' shared runtime helper compilation',[compiler,'-std=c++17',
                '-Wall','-Wextra','-Wpedantic','-Werror',*sanitizer,*defines,
                '-I'+str(ROOT/'game_shared'),str(TESTS/'modular_asset_runtime_tests.cpp'),
                str(ROOT/'game_shared/modular_weapons.cpp'),'-o',str(binary)],environment)
            if built.returncode==0:outputs.append(check(side+' selection/damage/quantizer/resources/fingerprint assertions',[str(binary)],environment).stdout)
        results.append(len(outputs)==2 and outputs[0]==outputs[1]);print(('PASS ' if results[-1] else 'FAIL ')+'shared client/server helper agreement',flush=True)
        for script,label in [('runtime_j10_6_9_4_integration.py','real client/server context and NPC actions'),
                             ('sprites_j10_6_9_4_integration.py','real HUD sprites and fallback'),
                             ('ballistic_manager_j10_6_3_integration.py','real manager, impact, save and restore')]:
            check(label,[sys.executable,str(TESTS/script)],environment)
        delta=(ROOT/'game_dir/delta.lst').read_text()
        weapon=delta[delta.index('weapon_data_t none'):delta.index('event_t none')]
        wire=bool(re.search(r'DEFINE_DELTA\(\s*iuser4,.*10,',weapon)) and bool(re.search(r'DEFINE_DELTA\(\s*m_iId,.*6,',weapon))
        wire &= 'DecodeAssetState(data.fuser1' in (ROOT/'client/weapon_predicting_context.cpp').read_text()
        wire &= 'EncodeAssetState(mod->m_assetSeed' in (ROOT/'server/client.cpp').read_text()
        for field,scale in [('fuser1','1000'),('fuser2','128'),('fuser3','128')]:
            wire &= bool(re.search(r'DEFINE_DELTA\(\s*'+field+r',.*22,\s*'+scale+r'\.0',weapon))
        results.append(wire);print(('PASS ' if wire else 'FAIL ')+'actual delta and prediction field integration',flush=True)
        validator=temporary/'validate'
        built=check('native validator compilation',[compiler,'-std=c++17',*sanitizer,
            '-I'+str(ROOT/'game_shared'),str(ROOT/'tools/weapon_editor/native_validate.cpp'),
            str(ROOT/'game_shared/modular_weapons.cpp'),'-o',str(validator)],environment)
        if built.returncode==0:
            check('runtime sample definitions',[str(validator),str(ROOT/'documentation/examples/modular-runtime9_4')],environment)
        sprites=sorted((ROOT/'documentation/examples/modular-runtime9_4/sprites').glob('*.spr'))
        sprite_valid=len(sprites)==4
        for path in sprites:
            data=path.read_bytes();header=struct.unpack('<4s9i',data[:40])
            sprite_valid &= header[:4]==(b'IDSP',2,2,3) and header[7]==1
            sprite_valid &= len(data)==40+2+768+20+header[5]*header[6]
            sprite_valid &= struct.unpack('<H',data[40:42])[0]==256
            frame=struct.unpack('<5i',data[810:830])
            sprite_valid &= frame[0]==0 and frame[3:5]==header[5:7]
        for path in (ROOT/'documentation/examples/modular-runtime9_4/sprites').glob('*.txt'):
            sprite_valid &= path.read_text().splitlines()[0]=='4'
        results.append(sprite_valid);print(('PASS ' if sprite_valid else 'FAIL ')+'four authored IDSP sprites and HUD list headers',flush=True)
        if sys.platform.startswith('linux'):
            base=[compiler,'-std=c++17','-fsyntax-only','-fno-exceptions','-Wno-write-strings',
                '-Wno-invalid- offsetof'.replace(' ',''),'-D_LINUX','-DLINUX',
                '-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp','-D_snprintf=snprintf','-D_vsnprintf=vsnprintf']
            common=['common','engine','game_shared','public']
            units={'server':['game_shared/weapons/modular.cpp','server/modular_ballistics.cpp','server/modular_npc.cpp',
                'server/weapons/weapon_modular.cpp','server/weapons.cpp','server/client.cpp','server/player.cpp',
                'server/gamerules/multiplay_gamerules.cpp','server/server_weapon_layer_impl.cpp','server/combat.cpp'],
                'client':['game_shared/weapons/modular.cpp','client/ammo.cpp','client/death.cpp',
                          'client/client_weapon_layer_impl.cpp','client/weapon_predicting_context.cpp']}
            for side,paths in units.items():
                defines=['-DCLIENT_DLL','-DCLIENT_WEAPONS'] if side=='client' else []
                includes=common+(['client','client/render','client/visualizer','client/events','client/ui'] if side=='client' else
                    ['server','server/gamerules','server/entities','server/monsters','server/weapons'])
                for unit in paths:
                    check('complete '+side+' syntax: '+unit,base+defines+['-I'+str(ROOT/p) for p in includes]+[str(ROOT/unit)],environment)
        if not args.skip_inherited:
            flags=['--ubsan'] if args.ubsan else []
            check('inherited corrected J.10.6.9.3 acceptance',[sys.executable,str(TESTS/'verify_j10_6_9_3.py'),*flags],environment)
    print(f'J.10.6.9.4 available check groups: {sum(results)}/{len(results)} passed',flush=True)
    print('Full Windows/client/server/PhysX linking and live network/save/HUD/sky maps require separate acceptance.',flush=True)
    return 0 if all(results) else 1

if __name__=='__main__':raise SystemExit(main())
