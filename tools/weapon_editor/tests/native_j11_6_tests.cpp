#include "pxwe_project.h"
#include "modular_asset_selection.h"
#include "modular_npc_cadence.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace pxwe;
namespace {
void Check(bool yes, const char* why) { if (!yes) throw std::runtime_error(why); }
void Put(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary); out << text;
}
std::string Read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
struct Temp {
    fs::path root = fs::temp_directory_path() / ("pxwe-j116-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() { fs::create_directories(root); }
    ~Temp() { std::error_code ec; fs::remove_all(root, ec); }
};
}
int main() {
    try {
        {
            Temp temp; Project editor; editor.Load(temp.root);
            editor.Add(Kind::Cartridge, "new9"); editor.Add(Kind::Weapon, "newgun");
            const auto* ammo = editor.Find(Kind::Cartridge,"new9");
            const auto* gun = editor.Find(Kind::Weapon,"newgun");
            Check(ammo && gun && ammo->Get("version")=="4" && gun->Get("version")=="4", "New must use v4");
            Check(ammo->Get("attack_logic")=="hitscan" && ammo->Get("damage_logic")=="legacy" &&
                  ammo->Get("default_damage")=="20", "v4 default cartridge incomplete");
            Check(gun->Get("primary_behavior")=="fire_loaded_cartridge" &&
                  !gun->Get("default_fire_rate").empty() && !gun->accepted.empty(), "v4 default weapon incomplete");
            Check(editor.Validate().Ok(),"v4 New templates did not validate together");
            Check(editor.Validate().Message().find("Engine admission: UNKNOWN")!=std::string::npos,"editor falsely certifies engine admission");
            RuntimeAdmission impossible; impossible.ammoSlots=32; impossible.usedAmmoSlots=31;
            auto failed=editor.Validate(impossible);
            Check(!failed.Ok() && failed.admissionKnown && !failed.admissionPassed, "occupied ammo slots ignored");
            RuntimeAdmission available; available.ammoSlots=32;
            auto passed=editor.Validate(available);
            Check(passed.Ok() && passed.admissionKnown && passed.admissionPassed, "known engine registry not accepted");
            available.occupiedWeaponIds={std::stoi(gun->Get("runtime_id"))};
            Check(!editor.Validate(available).Ok(), "registered weapon ID conflict ignored");
            Check(!editor.Validate().warnings.empty(), "no missing loose resource warning");
            editor.Save(); Project again; again.Load(temp.root);
            Check(again.Validate().Ok(), "v4 generated assets did not round-trip");
            std::cout << "PASS v4 creation, script versus engine admission, missing loose/VFS warnings\n";
        }
        {
            Temp temp; Project p; p.Load(temp.root);
            p.AddLegacy(Kind::Cartridge,"old9"); p.AddLegacy(Kind::Weapon,"oldgun");
            Check(p.Find(Kind::Cartridge,"old9")->Get("version")=="2" &&
                  p.Find(Kind::Weapon,"oldgun")->Get("version")=="2" && p.Validate().Ok(),
                  "explicit old-format creation must remain valid");
            std::cout << "PASS explicit legacy templates\n";
        }
        {
            modular::Weapon weapon; weapon.schemaVersion=4; weapon.id="test";
            weapon.defaultFireSound="weapons/default.wav";
            weapon.assets.push_back({"cartridge","primary_sound","weapons/conditional.wav","special",modular::AssetCondition::Cartridge});
            weapon.assets.push_back({"auto","primary_sound","weapons/auto.wav","auto",modular::AssetCondition::FireMode});
            weapon.assets.push_back({"random1","primary_sound","weapons/random1.wav","",modular::AssetCondition::Random});
            weapon.assets.push_back({"random2","primary_sound","weapons/random2.wav","",modular::AssetCondition::Random});
            const modular::AssetSelection specific{"special","semi",42,modular::AssetAction::Primary};
            const modular::AssetSelection generic{"other","auto",42,modular::AssetAction::Primary};
            const modular::AssetSelection rand{"other","semi",42,modular::AssetAction::Primary};
            Check(modular::SelectAsset(weapon,specific,"primary_sound","fire_sound")=="weapons/conditional.wav", "v4 conditional sound ignored");
            Check(modular::SelectAsset(weapon,generic,"primary_sound","fire_sound")=="weapons/auto.wav", "v4 mode sound ignored");
            const auto random = modular::SelectAsset(weapon,rand,"primary_sound","fire_sound");
            Check(random==modular::SelectAsset(weapon,rand,"primary_sound","fire_sound") &&
                  (random=="weapons/random1.wav"||random=="weapons/random2.wav"), "random sound unstable");
            weapon.ammoOverrides.push_back({}); weapon.ammoOverrides.back().cartridgeId="special";
            weapon.ammoOverrides.back().fireSound="weapons/override.wav";
            Check(modular::SelectAsset(weapon,specific,"primary_sound","fire_sound")=="weapons/override.wav","direct override must win");
            weapon.assets.clear(); weapon.ammoOverrides.clear();
            Check(modular::SelectAsset(weapon,specific,"primary_sound","fire_sound")=="weapons/default.wav","default v4 sound ignored");
            Check(modular::SelectAsset(weapon,specific,"secondary_sound","fire_sound")=="weapons/default.wav","secondary default sound ignored");
            weapon.schemaVersion=3; weapon.fireSound="weapons/old.wav";
            Check(modular::SelectAsset(weapon,specific,"primary_sound","fire_sound")=="weapons/old.wav","old sound fallback changed");
            std::cout << "PASS v4 ammo/mode/random/direct/default sound precedence and legacy fallback\n";
        }
        {
            Temp temp; Project p; p.Load(temp.root); p.Add(Kind::Cartridge,"new9"); p.Save();
            auto file=temp.root/"scripts/cartridges/new9.cartridge";
            auto old=Read(file);
            auto stage=temp.root/".pxwe-stage-test";
            Put(stage/"targets.txt","scripts/cartridges/new9.cartridge\nscripts/weapons/foreign.weapon\n");
            Put(stage/"originals.txt","scripts/cartridges/new9.cartridge\n");
            fs::create_directories(stage/"backup/scripts/cartridges");
            fs::rename(file,stage/"backup/scripts/cartridges/new9.cartridge");
            Put(file,"partially-installed");
            Put(temp.root/"scripts/weapons/foreign.weapon","newly-installed");
            Check(Project::PendingRecovery(temp.root).size()==1,"interrupted stage not detected");
            bool refused=false; try {p.Load(temp.root);} catch (const std::exception&) {refused=true;}
            Check(refused,"editor loaded incomplete transaction");
            Project::RecoverInterruptedSave(temp.root);
            Check(Read(file)==old && !fs::exists(temp.root/"scripts/weapons/foreign.weapon") &&
                  !fs::exists(stage),"recovery did not restore exactly previous files");
            p.Load(temp.root); Check(p.Validate().Ok(),"project failed after recovery");
            std::cout<<"PASS interrupted-save journal detects, blocks open, restores originals and removes new files\n";
        }
        {
            modular::NpcFireCadence npc;
            const std::uintptr_t entity=0x1000;
            Check(npc.Ready(entity,1,"smg",10.0),"first NPC shot blocked");
            npc.Record(entity,1,"smg",10.0,600.0);
            Check(!npc.Ready(entity,1,"smg",10.05) && npc.Ready(entity,1,"smg",10.1),"NPC 600 RPM upper bound incorrect");
            npc.Record(entity,1,"smg",10.1,300.0);
            Check(!npc.Ready(entity,1,"smg",10.21) && npc.Ready(entity,1,"smg",10.3),"NPC 300 RPM upper bound incorrect");
            Check(npc.Ready(entity,2,"smg",10.11),"new edict serial inherited cooldown");
            Check(npc.Ready(entity,1,"rifle",10.11),"different weapon inherited cooldown");
            Check(npc.Ready(entity,1,"smg",0.1),"level time reset inherited cooldown");
            std::cout << "PASS NPC RPM cap, ammo rate changes, serial/weapon/time resets\n";
        }
        std::cout << "5/5 J.11.6 groups passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL J.11.6: " << e.what() << "\n"; return 1;
    }
}
