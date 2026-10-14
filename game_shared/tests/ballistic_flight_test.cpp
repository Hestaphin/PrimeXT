// Build without engine dependencies to check SI physics independently.
#include "ballistic_flight.h"
#include "modular_weapons.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc,char**argv) {
    using namespace modular;
    BallisticParameters p{};
    p.massKg=.008; p.diameterM=.009; p.dragCoefficient=0;
    BallisticState s{}; s.velocity={360,0,0};
    for(int i=0;i<100;++i) s=IntegrateBallistic(s,p,.01);
    assert(std::abs(s.position.x-360)<.001);
    assert(std::abs(s.position.z + 4.903325) < .03);
    assert(std::abs(s.velocity.z + 9.80665) < .02);
    BallisticParameters p2=p; p2.massKg=.020;
    BallisticState heavier{}; heavier.velocity={360,0,0};
    for(int i=0;i<100;++i) heavier=IntegrateBallistic(heavier,p2,.01);
    assert(std::abs(heavier.position.z-s.position.z)<.0001); // mass-independent gravity
    BallisticParameters drag=p; drag.dragCoefficient=.30;
    BallisticState light{}; light.velocity={360,0,0};
    for(int i=0;i<100;++i) light=IntegrateBallistic(light,drag,.01);
    drag.massKg=.016;
    BallisticState heavy{}; heavy.velocity={360,0,0};
    for(int i=0;i<100;++i) heavy=IntegrateBallistic(heavy,drag,.01);
    assert(heavy.position.x > light.position.x && light.position.x < s.position.x);
    assert(BallisticEnergy(light,.008) < BallisticEnergy(BallisticState{{},{360,0,0}},.008));
    assert(IntegrateBallistic(light,drag,2).seconds == light.seconds); // bad dt rejected
    BallisticParameters extreme{};
    extreme.massKg = .000001; extreme.diameterM = 1; extreme.dragCoefficient = 20;
    BallisticState fast{}; fast.velocity={10000,0,0};
    const auto slowed=IntegrateBallistic(fast,extreme,.005);
    assert(std::isfinite(slowed.velocity.x) && slowed.velocity.x>=0 && slowed.velocity.x<fast.velocity.x);
    assert(argc==2);
    std::string root=argv[1];
    ReadFile fromDisk=[&](const std::string& file,std::string& data) {
        std::ifstream stream(root+"/"+file);
        if (!stream) return false;
        std::ostringstream out;out<<stream.rdbuf();data=out.str();return true;
    };
    Registry registry;
    assert(registry.Load(fromDisk));
    auto weapon=registry.FindWeapon("test_pistol");
    assert(weapon && weapon->primary.behavior=="ballistic");
    assert(weapon->secondary.behavior=="ammo_cycle");
    assert(weapon->secondary.ammoCost==0);
    assert(registry.AmmoReserveLimit("test_jhp")==weapon->maxCarry);
    auto cart=registry.FindCartridge("test_jhp");
    assert(cart && cart->schemaVersion==2 && cart->damage>0);
    assert(SerializeCartridge(*cart).find("damage = ")!=std::string::npos);
    // Invalid v2 damage and illegal ammo-cycle costs must fail atomically.
    auto broken=[&](const std::string& path,const std::string& old,const std::string& replacement) {
        Registry r;
        assert(!r.Load([&](const std::string& file,std::string& value) {
            if(!fromDisk(file,value)) return false;
            if(file==path) { auto at=value.find(old);assert(at!=std::string::npos);value.replace(at,old.size(),replacement); }
            return true;
        }));
        assert(r.Weapons().empty() && r.Cartridges().empty());
    };
    broken("scripts/cartridges/test_jhp.cartridge","damage = 18","damage = nan");
    broken("scripts/weapons/test_pistol.weapon","secondary_ammo_cost = 0","secondary_ammo_cost = 1");
    broken("scripts/weapons/test_pistol.weapon","secondary_behavior = ammo_cycle","secondary_behavior = unknown");
    std::cout<<"PASS: gravity/mass independence, air drag, kinetic energy, v2 cartridges, selection validation (3 rejection cases)\n";
}
