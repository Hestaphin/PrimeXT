#!/usr/bin/env python3
"""Compile the real J.11.3 player/NPC firing entry points on client and server.
Assertions exercise spread, RPM, audio, selected-versus-loaded ammo and burst cadence.
"""
from pathlib import Path
import os,subprocess,tempfile
from pose_j10_6_2_integration import function
import runtime_j11_2_integration as old
ROOT=old.ROOT
TEMPLATE=old.TEMPLATE.read_text()
MAIN=old.MAIN
# Retain J.11.2 correctness tests. Add new assertions at the same cartridge switch points.
MAIN=MAIN.replace('''    assert(weapon->compatible.size()==3);''','''    assert(weapon->compatible.size()==3);
    const auto normal=modular::ResolveFireSettings(*weapon,"hitscan9");
    const auto overridden=modular::ResolveFireSettings(*weapon,"ballistic9");
    const auto inherited=modular::ResolveFireSettings(*weapon,"energy9");
    assert(normal.rateRpm==600 && std::abs(normal.SecondsBetweenShots()-0.1)<1.e-6);
    assert(overridden.rateRpm==480 && std::abs(overridden.SecondsBetweenShots()-0.125)<1.e-6);
    assert(overridden.spread==0 && overridden.sound=="weapons/smg_suppressed.wav");
    assert(inherited.rateRpm==600 && inherited.sound==normal.sound);
    assert(normal.spread > overridden.spread);
''')
MAIN=MAIN.replace('''    assert(io->shots==1 && io->lastDamage==60 && io->lastBulletType==2);''','''    assert(io->shots==1 && io->lastDamage==60 && io->lastBulletType==2);
    assert(std::abs(io->lastSpread-0.02f)<1.e-6);
#ifndef CLIENT_DLL
    assert(lastSound=="weapons/smg.wav");
#endif
''')
MAIN=MAIN.replace('''    player.m_loadedCartridgeIndex=1;
    const int clip''','''    // Merely SELECTING ballistic ammo did not change existing hitscan spread.
    assert(std::abs(io->lastSpread-0.02f)<1.e-6);
    player.m_loadedCartridgeIndex=1;
    const int clip''')
MAIN=MAIN.replace('''    assert(launchedSelection.cartridgeId=="ballistic9");''','''    assert(launchedSelection.cartridgeId=="ballistic9");
    assert(lastBallisticSpread == 0);
    assert(lastSound == "weapons/smg_suppressed.wav");''')
MAIN=MAIN.replace('''    assert(io->lastDamage==40 && io->lastBulletType==1);''','''    assert(io->lastDamage==40 && io->lastBulletType==1);
    assert(std::abs(io->lastSpread)<1.e-6f);''')
MAIN=MAIN.replace('''    assert(npc.bullets==1 && npc.damage==40 && launched==prior);''','''    assert(npc.bullets==1 && npc.damage==40 && launched==prior);
    assert(std::abs(npc.lastSpread.x)<1.e-6f);
    assert(lastSound=="weapons/smg_suppressed.wav");''')
MAIN=MAIN.replace('''    assert(npc.bullets==2 && npc.damage==60);''','''    assert(npc.bullets==2 && npc.damage==60);
    assert(std::abs(npc.lastSpread.x-0.02f)<1.e-6);
    assert(lastSound == "weapons/smg.wav");''')
MAIN=MAIN.replace('''    assert(launched==prior+1 && npc.bullets==3);''','''    assert(launched==prior+1 && npc.bullets==3);
    assert(std::abs(lastBallisticSpread-0.02f)<1.e-6);
    assert(lastSound=="weapons/smg.wav");''')
MAIN=MAIN.replace('''    std::cout<<"PASS J.11.2 v4 loaded ammo player/NPC hitscan & ballistic refusal\\n";''','''    // Both Trigger and ContinueBurst must use the LOADED ammo RPM, including
    // selection of a different cartridge for the next reload.
    player.m_iClip=10;
    player.m_loadedCartridgeIndex=0;
    player.m_selectedCartridgeIndex=1;
    player.Trigger(false);
    assert(std::abs(player.m_flNextPrimaryAttack-1.1f)<1.e-5f);
    player.m_loadedCartridgeIndex=1;
    player.Trigger(false);
    assert(std::abs(player.m_flNextPrimaryAttack-1.125f)<1.e-5f);
    player.m_burstRemaining=2;
    player.m_flNextPrimaryAttack=0;
    player.ContinueBurst();
    assert(std::abs(player.m_flNextPrimaryAttack-1.125f)<1.e-5f);
    assert(player.m_burstRemaining==1);
    std::cout<<"PASS J.11.3 loaded ammunition effective RPM, spread, sound, trigger/burst prediction\\n";''')
# Hook observability into test adapters.
TEMPLATE=TEMPLATE.replace('''struct CBaseEntity {int serialnumber=1;entvars_t vars;entvars_t* pev=&vars;float damage=0;int bullets=0;''','''struct CBaseEntity {int serialnumber=1;entvars_t vars;entvars_t* pev=&vars;float damage=0;int bullets=0;Vector lastSpread{};''')
TEMPLATE=TEMPLATE.replace('''void FireBullets(int n,Vector,Vector,Vector,float,int,int,float d=0){bullets+=n;damage=d;}''','''void FireBullets(int n,Vector,Vector,Vector spread,float,int,int,float d=0){bullets+=n;damage=d;lastSpread=spread;}''')
TEMPLATE=TEMPLATE.replace('''int ammo[32]{},buttons=0,water=0,shots=0,events=0,animation=-1;''','''int ammo[32]{},buttons=0,water=0,shots=0,events=0,animation=-1,lastBulletType=0;float lastSpread=0;''')
TEMPLATE=TEMPLATE.replace('''float,float,int,uint32_t,float damage=0){shots+=count;lastDamage=damage;''','''float,float spread,int bulletType,uint32_t,float damage=0){shots+=count;lastDamage=damage;lastBulletType=bulletType;lastSpread=spread;''')
TEMPLATE=TEMPLATE.replace('''bool ballistics=true,admit=true;int launched=0;''','''bool ballistics=true,admit=true;int launched=0;float lastBallisticSpread=-1;''')
TEMPLATE=TEMPLATE.replace('    const Vector&,const matrix3x3&,float,uint32_t,const modular::AssetSelection* selection){\n    if(!admit)return false;launched+=count;launchedSelection=*selection;return true;','    const Vector&,const matrix3x3&,float spread,uint32_t,const modular::AssetSelection* selection){\n    if(!admit)return false;launched+=count;launchedSelection=*selection;lastBallisticSpread=spread;return true;')

def main():
    template=TEMPLATE[:TEMPLATE.index('int main(int argc,char** argv)')]+MAIN
    shared=(ROOT/'game_shared/weapons/modular.cpp').read_text()
    production=shared[shared.index('namespace {'):]
    header=(ROOT/'game_shared/weapons/modular.h').read_text()
    declaration=function(header,'class CModularWeaponContext final')+';'
    declaration=declaration.replace('private:','public:')
    base=(ROOT/'game_shared/weapon_context.cpp').read_text()
    methods='\n'.join(function(base,sig) for sig in ['bool CBaseWeaponContext :: DefaultDeploy','BOOL CBaseWeaponContext :: DefaultReload'])
    npc=function((ROOT/'server/modular_npc.cpp').read_text(),'int FireModularNpcWeapon')
    with tempfile.TemporaryDirectory(prefix='j113_runtime_') as tmp:
        directory=Path(tmp)
        code=template.replace('@CONTEXT_DECLARATION@',declaration).replace('@BASE_METHODS@',methods).replace('@PRODUCTION@',production).replace('@NPC_PRODUCTION@',npc)
        (directory/'runtime.cpp').write_text(code)
        for side,flags in [('server',[]),('client',['-DCLIENT_DLL'])]:
            binary=directory/side
            for cmd in [[os.environ.get('CXX','c++'),'-std=c++17','-O0','-g',*flags,
                         '-I'+str(ROOT/'game_shared'),'-I'+str(ROOT/'server'),str(directory/'runtime.cpp'),
                         str(ROOT/'game_shared/modular_weapons.cpp'),'-o',str(binary)],
                        [str(binary),str(ROOT/'documentation/examples/modular-v4-j11_1')]]:
                result=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
                if result.returncode: raise RuntimeError(f'{side}: {cmd}\n{(result.stdout+result.stderr)[-10000:]}')
                if result.stdout.strip():print(side+': '+result.stdout.strip(),flush=True)
if __name__=='__main__':main()
