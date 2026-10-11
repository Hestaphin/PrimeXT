#include "modular_asset_selection.h"
#include "modular_asset_state.h"
#include "modular_asset_resources.h"
#include "cartridge_damage.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

int main() {
    using namespace modular;
    int cases=0;
    auto check=[&](bool condition) { assert(condition); ++cases; };
    Weapon w; w.id="probe"; w.viewModel="flat.mdl"; w.fireSound="flat.wav";
    w.primary.sound="primary.wav"; w.secondary.sound="secondary.wav";
    AssetSelection s{"fmj","semi",0xfedcba98u,AssetAction::Primary};
    check(SelectAsset(w,s,"view_model")=="flat.mdl");
    check(SelectAsset(w,s,"primary_sound","fire_sound")=="primary.wav");
    check(SelectAsset(w,s,"secondary_sound","fire_sound")=="secondary.wav");
    w.assets.push_back({"default","view_model","default.mdl","",AssetCondition::Default});
    check(SelectAsset(w,s,"view_model")=="default.mdl");
    w.assets.push_back({"random_a","view_model","a.mdl","",AssetCondition::Random});
    w.assets.push_back({"random_b","view_model","b.mdl","",AssetCondition::Random});
    const auto pinned=SelectAsset(w,s,"view_model");
    check(pinned=="a.mdl" || pinned=="b.mdl");
    bool a=false,b=false;
    for (uint32_t seed=0; seed<256; ++seed) {
        auto action=s; action.seed=seed;
        a|=SelectAsset(w,action,"view_model")=="a.mdl";
        b|=SelectAsset(w,action,"view_model")=="b.mdl";
    }
    check(a&&b);
    std::reverse(w.assets.begin(),w.assets.end());
    check(SelectAsset(w,s,"view_model")==pinned);
    for (int frame=0;frame<100;++frame) check(SelectAsset(w,s,"view_model")==pinned);
    w.assets.push_back({"mode","view_model","semi.mdl","semi",AssetCondition::FireMode});
    check(SelectAsset(w,s,"view_model")=="semi.mdl");
    w.assets.push_back({"ammo","view_model","fmj.mdl","fmj",AssetCondition::Cartridge});
    check(SelectAsset(w,s,"view_model")=="fmj.mdl");
    AmmoOverride override; override.cartridgeId="fmj"; override.viewModel="override.mdl";
    override.fireSound="override.wav"; override.reloadSound="reload.wav";
    w.ammoOverrides.push_back(override);
    check(SelectAsset(w,s,"view_model")=="override.mdl");
    check(SelectAsset(w,s,"secondary_sound","fire_sound")=="override.wav");
    check(SelectAsset(w,s,"reload_sound")=="reload.wav");
    auto other=s; other.cartridgeId="jhp";
    check(SelectAsset(w,other,"view_model")=="semi.mdl");
    w.assets.push_back({"sound_default","primary_sound","default.wav","",AssetCondition::Default});
    w.assets.push_back({"sound_mode","fire_sound","semi.wav","semi",AssetCondition::FireMode});
    check(SelectAsset(w,other,"primary_sound","fire_sound")=="semi.wav");
    w.assets.push_back({"kill","kill_sprite","sprites/k.spr","",AssetCondition::Default});
    auto ref=ReferenceKillAsset(w,"sprites/k.spr");
    check(ref.kind==1&&ResolveKillAsset(w,ref)=="sprites/k.spr");
    w.ammoOverrides[0].killSprite="sprites/ammo_k.spr";
    ref=ReferenceKillAsset(w,"sprites/ammo_k.spr");
    check(ref.kind==2&&ResolveKillAsset(w,ref)=="sprites/ammo_k.spr");
    check(ResolveKillAsset(w,{1,999}).empty());
    check(ResolveKillAsset(w,{7,0}).empty());
    check(ReferenceKillAsset(w,"unregistered.spr").kind==0);
    for (uint32_t seed : {0u,2047u,2048u,0x80000000u,0xffffffffu})
        for (int action=0;action<=6;++action) {
            float f1,f2,f3; EncodeAssetState(seed,action,f1,f2,f3);
            check(f1*1000 <= 2097151 && f2*128 <= 2097151 && f3*128 <= 2097151);
            uint32_t decoded=0;int tag=0;
            check(DecodeAssetState(f1,f2,f3,decoded,tag)&&decoded==seed&&tag==action);
        }
    uint32_t decoded;int tag;
    check(!DecodeAssetState(std::numeric_limits<float>::quiet_NaN(),0,0,decoded,tag));
    check(!DecodeAssetState(0,0,896,decoded,tag));
    check(!DecodeAssetState(2048,0,0,decoded,tag));
    check(!DecodeAssetState(.5,0,0,decoded,tag));
    Cartridge c;c.schemaVersion=3;c.massGrams=8;c.diameterMm=9;c.damageCoefficient=2;
    const double muzzle=518.4/(3.14159265358979323846*81/4)*2;
    check(std::abs(MuzzleDamage(c,360)-muzzle)<1e-9);
    check(std::abs(MuzzleDamage(c,180)-muzzle*.25)<1e-9);
    check(std::abs(ImpactDamage(518.4*.01,518.4,9,2,20)-muzzle*.01)<1e-9);
    check(ImpactDamage(0,518.4,9,2,20)==0);
    check(std::abs(ImpactDamage(518.4*.01,518.4,9,0,20)-1)<1e-9);
    c.schemaVersion=2;c.damage=17;check(MuzzleDamage(c,360)==17&&MuzzleDamage(c,180)==17);
    const auto paths=ConditionalAssetPaths(w);
    check(paths.count("override.wav")&&paths.count("sprites/ammo_k.spr")&&paths.count("a.mdl"));
    const auto sprites=SpriteListResources("2\nweapon 640 hud7 0 0 32 16\nammo 320 ammo/icon 0 0 8 8 // OK\nweapon 640 ../escape 0 0 1 1\n");
    check(sprites.size()==2&&sprites.count("sprites/hud7.spr")&&sprites.count("sprites/ammo/icon.spr"));
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    auto load=[&](Registry& registry,const char* name) {
        return registry.Load([&](const std::string& path,std::string& text) {
            std::ifstream file(root/"documentation/examples"/name/path);
            if (!file) return false;
            std::ostringstream buffer; buffer << file.rdbuf(); text=buffer.str(); return true;
        });
    };
    Registry legacy,modern;
    check(load(legacy,"modular-ballistics")&&legacy.Fingerprint()==1605249880u);
    check(legacy.RuntimeFingerprint()==legacy.Fingerprint());
    check(load(modern,"modular-format3")&&modern.Fingerprint()==3608015818u);
    check(modern.RuntimeFingerprint()!=modern.Fingerprint());
    Registry copy;check(load(copy,"modular-format3")&&copy.RuntimeFingerprint()==modern.RuntimeFingerprint());
    std::cout<<"PASS "<<cases<<" asset, damage, quantizer, resource and runtime-handshake assertions\n";
    std::cout<<"Fingerprints: legacy="<<legacy.Fingerprint()<<" format3="<<modern.Fingerprint()
             <<" runtime="<<modern.RuntimeFingerprint()<<"\n";
}
