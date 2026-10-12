// J.11.3: native pxwe Version 4 override editing and game-effective RPM/spread/sound.
#include "pxwe_project.h"
#include "pxwe_properties.h"
#include "modular_weapons.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <chrono>
namespace fs = std::filesystem;
static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const auto source=fs::path(argv[1]);
    const auto root=fs::temp_directory_path()/ ("j113-editor-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directory(root);
        fs::copy(source,root,fs::copy_options::recursive);
        pxwe::Project p; p.Load(root);
        Check(p.Validate().Ok(),"Unable to load version 4 example");
        auto validated=p.Validate();
        const auto* w=validated.registry.FindWeapon("schema_smg"); Check(w,"No v4 example weapon");
        auto settings=modular::ResolveFireSettings(*w,"hitscan9");
        Check(settings.rateRpm==600 && settings.spread==.02f && settings.sound=="weapons/smg.wav","Defaults not selected");
        settings=modular::ResolveFireSettings(*w,"ballistic9");
        Check(settings.rateRpm==480 && settings.spread==0 && settings.sound=="weapons/smg_suppressed.wav","Original ammo override not selected");
        const auto before=p.Validate().registry.Fingerprint();
        p.SetAmmoOverrides("schema_smg","energy9",{{"fire_rate","900"},{"spread","0"},{"fire_sound","weapons/energy.wav"}});
        Check(p.Validate().Ok()&&p.Find(pxwe::Kind::Weapon,"schema_smg")->Get("version")=="4", "Editor downgraded new schema");
        validated=p.Validate(); w=validated.registry.FindWeapon("schema_smg"); Check(w,"Edited weapon missing");
        settings=modular::ResolveFireSettings(*w,"energy9");
        Check(settings.rateRpm==900 && settings.spread==0 && settings.sound=="weapons/energy.wav", "Three overrides not applied");
        Check(std::abs(settings.SecondsBetweenShots() - 1.0/15.0)<1e-7, "900 RPM incorrect");
        p.SetField(pxwe::Kind::Weapon,"schema_smg","default_fire_rate","720");
        validated=p.Validate(); w=validated.registry.FindWeapon("schema_smg");
        Check(modular::ResolveFireSettings(*w,"hitscan9").rateRpm==720 && modular::ResolveFireSettings(*w,"ballistic9").rateRpm==480 && modular::ResolveFireSettings(*w,"energy9").rateRpm==900,"Weapon default improperly replaced custom values");
        p.SetAmmoOverrides("schema_smg","energy9",{{"fire_rate",""},{"spread",""},{"fire_sound",""}});
        Check(p.Validate().Ok(),"Clear overrides invalid");
        validated=p.Validate(); w=validated.registry.FindWeapon("schema_smg");
        settings=modular::ResolveFireSettings(*w,"energy9");
        Check(settings.rateRpm==720 && settings.spread==.02f && settings.sound=="weapons/smg.wav","Cleared overrides did not inherit");
        Check(p.Undo(),"Undo must restore override transaction");
        validated=p.Validate(); w=validated.registry.FindWeapon("schema_smg");
        settings=modular::ResolveFireSettings(*w,"energy9");
        Check(settings.rateRpm==900 && settings.spread==0,"Undo lost custom zero spread");
        p.Save(); pxwe::Project reload;reload.Load(root);
        Check(reload.Validate().Ok() && reload.Validate().registry.Fingerprint()==p.Validate().registry.Fingerprint(),"Editor saved different runtime state");
        const auto* asset=reload.Find(pxwe::Kind::Weapon,"schema_smg");
        Check(asset && asset->Get("ammo.energy9.spread")=="0","Explicit zero lost on disk");
        Check(pxwe::DescribeProperty(pxwe::Kind::Weapon,"default_fire_sound",asset).resource==pxwe::ResourceType::Sound,"Default Fire Sound lacks WAV browse");
        const auto* cartridge=reload.Find(pxwe::Kind::Cartridge,"hitscan9");
        Check(cartridge,"No Hitscan cartridge");
        Check(pxwe::DescribeProperty(pxwe::Kind::Cartridge,"base_damage",cartridge).readOnly,"Inactive Base Damage not disabled");
        auto rows=pxwe::PropertyRows(*asset,{}); bool found=false;
        for (const auto& row: rows) if(row.key=="default_fire_rate") found=true;
        Check(found,"Default Fire Rate missing from v4 properties");
        Check(reload.Validate().registry.Fingerprint()!=before,"Overrides made no gameplay change");
        bool rejected=false;
        try {reload.SetAmmoOverrides("schema_smg","energy9",{{"fire_rate","nan"}});} catch(const std::exception&){rejected=true;}
        Check(rejected,"NaN RPM was permitted");
        rejected=false;
        try {reload.SetAmmoOverrides("schema_smg","energy9",{{"spread","-0.1"}});} catch(const std::exception&){rejected=true;}
        Check(rejected,"Negative spread was permitted");
        rejected=false;
        try {reload.SetAmmoOverrides("schema_smg","energy9",{{"fire_rate","0"}});} catch(const std::exception&){rejected=true;}
        Check(rejected,"Zero RPM was permitted");
        std::cout<<"PASS J.11.3 editor overrides, defaults, zero spread, validation, undo, save/reload, property help\n";
        fs::remove_all(root);return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL J.11.3 editor: "<<e.what()<<'\n';fs::remove_all(root);return 1;
    }
}
