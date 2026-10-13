// J.11.1 schema regression, validated again with the J.11.2 runtime dispatcher.
// Build: c++ -std=c++17 -Igame_shared game_shared/modular_weapons.cpp game_shared/tests/modular_j11_1_schema_tests.cpp -o schema_test
#include "modular_weapons.h"
#ifdef NDEBUG
#undef NDEBUG // Assertions here also execute test inputs in Release builds.
#endif
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
using namespace modular;

static const std::string common =
 "version = 4\nchamber = nine_mm\nprojectile = fmj\nmass_grams = 8\n"
 "diameter_mm = 9\ndrag_coefficient = 0.3\nreference_velocity_mps = 360\nprojectile_count = 1\n";
static const std::string hit = common+
 "id = h\nattack_logic = hitscan\ndamage_logic = legacy\ndefault_damage = 60\nlegacy_bullet_type = 2\n";
static const std::string base = common+
 "id = b\nattack_logic = ballistic\ndamage_logic = configurable_base\nbase_damage = 40\n";
static const std::string energy = common+
 "id = e\nattack_logic = ballistic\ndamage_logic = energy_based\ndamage_coefficient = 2\n";
static const std::string weapon =
 "version = 4\nid = gun\ndisplay_name = Test Gun\nammo_name = gun_ammo\n"
 "default_cartridge = h\nruntime_id = 32\nclip_size = 30\ndefault_ammo = 20\n"
 "max_carry = 90\nslot = 1\nposition = 0\nview_model = models/v_test.mdl\n"
 "player_model = models/p_test.mdl\nworld_model = models/w_test.mdl\n"
 "default_fire_rate = 600\ndefault_spread = 0.015\n"
 "default_fire_sound = weapons/test.wav\nreload_seconds = 1.5\nrange = 8192\n"
 "primary_behavior = fire_loaded_cartridge\nsecondary_behavior = none\n"
 "accept = h:360\naccept = b:360\naccept = e:360\n"
 "ammo.b.fire_rate = 900\nammo.b.spread = 0\n"
 "ammo.b.fire_sound = weapons/fast.wav\n";

static bool Replace(std::string& s, const std::string& a, const std::string& b) {
 auto p=s.find(a); if(p==std::string::npos) return false; s.replace(p,a.size(),b); return true;
}
static bool Load(Registry& r,const std::map<std::string,std::string>& files) {
 return r.Load([&](const std::string& f,std::string& data) {
  auto p=files.find(f); if(p==files.end()) return false; data=p->second; return true;
 });
}
int main(int argc, char** argv) {
 std::map<std::string,std::string> files = {
   {"scripts/cartridges/manifest.txt", "h\nb\ne\n"},
   {"scripts/weapons/manifest.txt", "gun\n"},
   {"scripts/cartridges/h.cartridge",hit},
   {"scripts/cartridges/b.cartridge",base},
   {"scripts/cartridges/e.cartridge",energy},
   {"scripts/weapons/gun.weapon",weapon}
 };
 Registry r;
 assert(Load(r,files));
 assert(r.Cartridges().size()==3 && r.Weapons().size()==1);
 assert(r.FindCartridge("h")->attackLogic==AttackLogic::Hitscan);
 assert(r.FindCartridge("b")->damageLogic==DamageLogic::ConfigurableBase);
 assert(r.FindCartridge("e")->damageLogic==DamageLogic::EnergyBased);
 const auto* w=r.FindWeapon("gun"); assert(w);
 assert(w->defaultFireRateRpm==600 && SecondsPerShot(w->defaultFireRateRpm)==.1);
 assert(w->ammoOverrides.size()==1);
 const auto& override=w->ammoOverrides[0];
 assert(override.fireRateRpm && *override.fireRateRpm==900);
 assert(override.spreadOverride && *override.spreadOverride==0);
 assert(override.fireSound=="weapons/fast.wav");
 assert(!w->fireSound.empty() && w->primary.behavior=="fire_loaded_cartridge");
 assert(w->ammoOverrides[0].cartridgeId=="b");
 std::string err;
 assert(r.ValidateForRuntime(32,0,{}, {},err));
 assert(err.empty());
 const auto fingerprint=r.Fingerprint();
 for(auto id:{"h","b","e"}) {
    files[std::string("scripts/cartridges/")+id+".cartridge"] = SerializeCartridge(*r.FindCartridge(id));
 }
 files["scripts/weapons/gun.weapon"]=SerializeWeapon(*w);
 assert(files["scripts/weapons/gun.weapon"].find("default_fire_rate = 600")!=std::string::npos);
 assert(files["scripts/weapons/gun.weapon"].find("fire_interval") == std::string::npos);
 assert(Load(r,files) && r.Fingerprint()==fingerprint);
 assert(r.RuntimeFingerprint()!=r.Fingerprint());
 assert(r.FindWeapon("gun")->ammoOverrides[0].spreadOverride==0.f);
 auto reject=[&](const std::string& path,const std::string& from,const std::string& to) {
   auto invalid=files; assert(Replace(invalid[path],from,to));
   assert(!Load(r,invalid)); assert(!r.Diagnostics().empty()); assert(r.Weapons().empty());
 };
 reject("scripts/cartridges/h.cartridge","attack_logic = hitscan","attack_logic = ballistic");
 reject("scripts/cartridges/b.cartridge","damage_logic = configurable_base","damage_logic = legacy");
 reject("scripts/cartridges/b.cartridge","base_damage = 40","base_damage = inf");
 reject("scripts/cartridges/b.cartridge","base_damage = 40","base_damage = NaN");
 reject("scripts/cartridges/b.cartridge","base_damage = 40","base_damage = 0");
 reject("scripts/cartridges/e.cartridge","damage_coefficient = 2","damage_coefficient = -1");
 reject("scripts/cartridges/h.cartridge","legacy_bullet_type = 2","legacy_bullet_type = 4");
 reject("scripts/cartridges/h.cartridge","default_damage = 60","default_damage = 60\nbase_damage = 3");
 reject("scripts/weapons/gun.weapon","default_fire_rate = 600","default_fire_rate = 0");
 reject("scripts/weapons/gun.weapon","default_fire_rate = 600","default_fire_rate = inf");
 reject("scripts/weapons/gun.weapon","default_fire_rate = 600","default_fire_rate = 600\nfire_interval = 0.1");
 reject("scripts/weapons/gun.weapon","ammo.b.fire_rate = 900","ammo.b.fire_rate = -1");
 reject("scripts/weapons/gun.weapon","ammo.b.spread = 0","ammo.b.spread = -0.1");
 reject("scripts/weapons/gun.weapon","ammo.b.spread = 0","ammo.not_accepted.spread = 0");
 reject("scripts/weapons/gun.weapon","primary_behavior = fire_loaded_cartridge","primary_behavior = typo");
 reject("scripts/weapons/gun.weapon","primary_behavior = fire_loaded_cartridge","primary_behavior = hitscan");
 {
   auto tooMany = files;
   assert(Replace(tooMany["scripts/cartridges/b.cartridge"],"projectile_count = 1","projectile_count = 5"));
   // 64 events times 5 projectiles would exceed the 256-per-trigger cap.
   assert(Replace(tooMany["scripts/weapons/gun.weapon"],"primary_behavior = fire_loaded_cartridge", "primary_behavior = fire_loaded_cartridge\nprimary_projectiles = 64"));
   assert(!Load(r,tooMany));
 }

 if (argc == 2) {
    const std::string root = argv[1];
    Registry sample;
    assert(sample.Load([&](const std::string& path, std::string& contents) {
        std::ifstream file(root + "/" + path);
        if (!file) return false;
        std::ostringstream buffer; buffer << file.rdbuf(); contents = buffer.str();
        return true;
    }));
    assert(sample.Cartridges().size() == 3 && sample.Weapons().size() == 1);
    assert(sample.FindWeapon("schema_smg")->ammoOverrides.size() == 1);
 }
 std::cout<<"PASS J.11.1/J.11.2 v4 schema roundtrip, RPM, overrides and runtime admission\n";
}
