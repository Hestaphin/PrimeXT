#!/usr/bin/env python3
"""J.11.2: exercise actual ballistic manager v4 impacts and save/restore."""
from pathlib import Path
import os
import subprocess
import tempfile
from pose_j10_6_2_integration import function

ROOT=Path(__file__).resolve().parents[2]
EXTRA=r'''
    // Load the v4 registry before testing the same real manager methods.
    assert(registry.Load([&](const std::string& path,std::string& text){
        std::ifstream file(std::string(argv[2])+"/"+path);
        if (!file) return false;
        std::ostringstream out;out<<file.rdbuf();text=out.str();return true;
    }));
    const auto& w4=*registry.FindWeapon("schema_smg");
    const auto& base=*registry.FindCartridge("ballistic9");
    const auto& ammoEnergy=*registry.FindCartridge("energy9");
    assert(w4.schemaVersion==4 && base.damageLogic==modular::DamageLogic::ConfigurableBase);
    assert(ammoEnergy.damageLogic==modular::DamageLogic::EnergyBased);
    CModularBallisticManager v4; globals.time=60; v4.Spawn();
    target.onDamage=nullptr;
    assert(v4.Add(&owner,w4,base,{0,0,0},{1,0,0},390));
    auto &b=v4.m_shots[0];
    assert(b.v4 && b.damageLogic==modular::DamageLogic::ConfigurableBase);
    assert(b.damage==40 && b.damageCoefficient==0);
    auto weak=b.state;weak.velocity.x*=0.5;
    Impact(b,hit,-1,{0,0,0},{1,0,0},weak);
    assert(near(target.damage,10));
    assert(v4.Save(write));
    assert(v4.m_savedDamageCoefficients[0]==0);
    v4.m_shots.clear();globals.time=61;
    assert(v4.Restore(read) && v4.Count()==1);
    assert(v4.m_shots[0].v4 && v4.m_shots[0].damageLogic==modular::DamageLogic::ConfigurableBase);
    Impact(v4.m_shots[0],hit,-1,{0,0,0},{1,0,0},weak);
    assert(near(target.damage,10));
    // Switching to an energy-based cartridge selects the other explicit formula.
    CModularBallisticManager e4;globals.time=70;e4.Spawn();
    assert(e4.Add(&owner,w4,ammoEnergy,{0,0,0},{1,0,0},420));
    assert(e4.m_shots[0].v4 && e4.m_shots[0].damageLogic==modular::DamageLogic::EnergyBased);
    auto weak2=e4.m_shots[0].state;weak2.velocity.x*=0.5;
    Impact(e4.m_shots[0],hit,-1,{0,0,0},{1,0,0},weak2);
    const double expected=modular::MuzzleDamage(ammoEnergy,420)*.25;
    assert(near(target.damage,expected));
    assert(e4.Save(write)); e4.m_shots.clear();globals.time=71;
    assert(e4.Restore(read) && e4.Count()==1);
    assert(e4.m_shots[0].v4 && e4.m_shots[0].damageLogic==modular::DamageLogic::EnergyBased);
    Impact(e4.m_shots[0],hit,-1,{0,0,0},{1,0,0},weak2);
    assert(near(target.damage,expected));
    std::cout<<"PASS J.11.2 actual v4 manager: base/energy impacts + save/restore\n";
'''

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
    disk_version=next(x for x in source.splitlines() if x.startswith('constexpr int kShotDiskVersion'))
    template=(ROOT/'game_shared/tests/fixtures/j10_6_3/ballistic_manager_adapter.cpp.in').read_text()
    template=template.replace('assert(argc==2);','assert(argc==3);')
    pos=template.rfind('\n}')
    assert pos!=-1
    template=template[:pos]+'\n'+EXTRA+template[pos:]
    with tempfile.TemporaryDirectory(prefix='j112_manager_') as temp:
        tmp=Path(temp);unit=tmp/'manager.cpp';binary=tmp/'manager'
        unit.write_text(template.replace('@SHOT_DECLARATION@',shot).replace('@PRODUCTION@',code)
            .replace('@DISK_DECLARATION@',disk).replace('@MANAGER_DECLARATION@',declaration)
            .replace('@DISK_VERSION@',disk_version))
        flags=['-fsanitize=undefined','-fno-sanitize-recover=all'] if os.environ.get('J112_UBSAN') else []
        args=[os.environ.get('CXX','c++'),'-std=c++17','-O0','-g',*flags,
            '-I'+str(ROOT/'game_shared'),'-I'+str(ROOT/'server'),str(unit),str(ROOT/'game_shared/modular_weapons.cpp'),
            *[str(ROOT/'game_shared'/s) for s in ['ballistic_flight.cpp','ballistic_penetration.cpp','ballistic_path_cache.cpp']],
            '-o',str(binary)]
        for command in [args,[str(binary),str(ROOT/'documentation/examples/modular-format3'),
                    str(ROOT/'documentation/examples/modular-v4-j11_1')]]:
            result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
            if result.returncode:raise RuntimeError((result.stdout+result.stderr)[-9000:])
            if result.stdout.strip():print(result.stdout.strip())
if __name__=='__main__':main()
