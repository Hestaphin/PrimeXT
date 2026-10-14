"""J.11.2 executable player/NPC cartridge dispatcher adapters (client + server)."""
from pathlib import Path
import os
import subprocess
import tempfile
from pose_j10_6_2_integration import function

ROOT=Path(__file__).resolve().parents[2]
TEMPLATE=ROOT/'game_shared/tests/fixtures/j10_6_9_4/runtime_adapter.cpp.in'
MAIN=r'''
int main(int argc,char** argv) {
    assert(argc==2);
    auto& registry=modular::SharedRegistry();
    assert(registry.Load([&](const std::string& path,std::string& text){
        std::ifstream file(std::string(argv[1])+"/"+path);
        if(!file)return false;
        std::ostringstream out;out<<file.rdbuf();text=out.str();
        if(path.find(".weapon")!=std::string::npos)text+="\nnpc_usable = 1\n";
        return true;
    }));
    std::string diagnostic;
    assert(registry.ValidateForRuntime(32,0,{}, {},diagnostic));
    auto* weapon=registry.FindWeapon("schema_smg");assert(weapon);
    assert(weapon->primary.behavior=="fire_loaded_cartridge");
    assert(weapon->compatible.size()==3);
    const auto* h=registry.FindCartridge("hitscan9");assert(h&&h->defaultDamage==60);
    const auto* b=registry.FindCartridge("ballistic9");assert(b&&b->baseDamage==40);
    const auto* e=registry.FindCartridge("energy9");assert(e);
    auto layer=std::make_unique<IWeaponLayer>();auto* io=layer.get();
    CModularWeaponContext player(std::move(layer),weapon);
    player.m_iClip=10;
    assert(player.FireAction(weapon->primary,false));
    assert(io->shots==1 && io->lastDamage==60 && io->lastBulletType==2);
    assert(player.m_iClip==9);
    // Selecting different ammo does NOT change the loaded magazine.
    player.m_selectedCartridgeIndex=1;
    assert(player.FireAction(weapon->primary,false));
    assert(io->shots==2 && io->lastDamage==60);
    player.m_loadedCartridgeIndex=1;
    const int clip=player.m_iClip, shots=io->shots;
#ifndef CLIENT_DLL
    // Disabled ballistic simulation uses instantaneous traces, not refusal.
    ballistics=false;
    assert(player.FireAction(weapon->primary,false));
    assert(player.m_iClip==clip-1 && io->shots==shots+1 && launched==0);
    assert(io->lastDamage==40 && io->lastBulletType==1);
    assert(launched==0 && lastSound=="weapons/smg_suppressed.wav");
    // Restoring the setting resumes simulated flight without editing the ammo.
    ballistics=true;
    assert(player.FireAction(weapon->primary,false));
    assert(launched==1 && io->shots==shots+1 && player.m_iClip==clip-2);
    assert(launchedSelection.cartridgeId=="ballistic9");
    player.m_loadedCartridgeIndex=2;
    const auto expectedEnergy=float(modular::MuzzleDamage(*e,weapon->compatible[2].muzzleVelocityMps));
    ballistics=false;
    assert(player.FireAction(weapon->primary,false));
    assert(io->shots==shots+2 && std::abs(io->lastDamage-expectedEnergy)<1.e-5f);
    ballistics=true;
    assert(player.FireAction(weapon->primary,false));
    assert(launched==2 && launchedSelection.cartridgeId=="energy9");
    // NPCs receive exactly the same disabled-ballistics fallback.
    CBaseEntity npc;
    ballistics=false;
    const int prior=launched;
    assert(FireModularNpcWeapon(&npc,"schema_smg","ballistic9",{},{1,0,0},10)==1);
    assert(npc.bullets==1 && npc.damage==40 && launched==prior);
    // NPC animation requesting another shot before the authored RPM delay
    // must not create damage, sound, or consume ammunition.
    const int soundBeforeCooldown = sounds;
    assert(FireModularNpcWeapon(&npc,"schema_smg","ballistic9",{},{1,0,0},10)==0);
    assert(npc.bullets==1 && sounds==soundBeforeCooldown);
    gpGlobals->time += 1;
    assert(FireModularNpcWeapon(&npc,"schema_smg","hitscan9",{},{1,0,0},10)==1);
    assert(npc.bullets==2 && npc.damage==60);
    assert(FireModularNpcWeapon(&npc,"schema_smg","energy9",{},{1,0,0},0)==0);
    gpGlobals->time += 1;
    assert(FireModularNpcWeapon(&npc,"schema_smg","energy9",{},{1,0,0},10)==1);
    assert(npc.bullets==3 && std::abs(npc.damage-expectedEnergy)<1.e-5f);
    ballistics=true;
    gpGlobals->time += 1;
    assert(FireModularNpcWeapon(&npc,"schema_smg","energy9",{},{1,0,0},10)==1);
    assert(launched==prior+1 && npc.bullets==3);
    assert(!g_modularDamageSource);
#else
    // Client predicts state and visuals only; it never launches a damage ray for ballistic.
    assert(player.FireAction(weapon->primary,false));
    assert(player.m_iClip==clip-1 && io->shots==shots);
    player.m_loadedCartridgeIndex=2;
    assert(player.FireAction(weapon->primary,false));
    assert(io->shots==shots);
#endif
    std::cout<<"PASS v4 loaded ammo player/NPC hitscan & ballistic fallback\n";
}
'''

def main():
    template=TEMPLATE.read_text()
    template=template[:template.index('int main(int argc,char** argv)')]+MAIN
    # Add observability for passing the cartridge legacy bullet type.
    template=template.replace('int ammo[32]{},buttons=0,water=0,shots=0,events=0,animation=-1;',
                              'int ammo[32]{},buttons=0,water=0,shots=0,events=0,animation=-1,lastBulletType=0;')
    template=template.replace('float,float,int,uint32_t,float damage=0){shots+=count;lastDamage=damage;',
                              'float,float,int bulletType,uint32_t,float damage=0){shots+=count;lastDamage=damage;lastBulletType=bulletType;')
    shared=(ROOT/'game_shared/weapons/modular.cpp').read_text()
    production=shared[shared.index('namespace {'):]
    header=(ROOT/'game_shared/weapons/modular.h').read_text()
    declaration=function(header,'class CModularWeaponContext final')+';'
    declaration=declaration.replace('private:','public:')
    base=(ROOT/'game_shared/weapon_context.cpp').read_text()
    methods='\n'.join(function(base,sig) for sig in ['bool CBaseWeaponContext :: DefaultDeploy','BOOL CBaseWeaponContext :: DefaultReload'])
    npc=function((ROOT/'server/modular_npc.cpp').read_text(),'int FireModularNpcWeapon')
    with tempfile.TemporaryDirectory(prefix='j112_runtime_') as tmp:
        p=Path(tmp)
        code=template.replace('@CONTEXT_DECLARATION@',declaration).replace('@BASE_METHODS@',methods).replace('@PRODUCTION@',production).replace('@NPC_PRODUCTION@',npc)
        (p/'runtime.cpp').write_text(code)
        for side,defines in [('server',[]),('client',['-DCLIENT_DLL'])]:
            binary=p/side
            flags=['-fsanitize=undefined','-fno-sanitize-recover=all'] if os.environ.get('J112_UBSAN') else []
            commands=[
                [os.environ.get('CXX','c++'),'-std=c++17','-O0','-g',*flags,*defines,
                 '-I'+str(ROOT/'game_shared'),'-I'+str(ROOT/'server'),str(p/'runtime.cpp'),
                 str(ROOT/'game_shared/modular_weapons.cpp'),'-o',str(binary)],
                [str(binary),str(ROOT/'documentation/examples/modular-v4-j11_1')]
            ]
            for cmd in commands:
                result=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
                if result.returncode:
                    raise RuntimeError(f'{side}: {cmd}\n{(result.stdout+result.stderr)[-9000:]}')
                if result.stdout.strip():print(side+': '+result.stdout.strip(),flush=True)
if __name__=='__main__':main()
