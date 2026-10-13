#include "pxwe_project.h"
#include "pxwe_properties.h"
#include <chrono>
#include <functional>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace pxwe;
namespace {
void Check(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); }
void Reject(const std::function<void()>& fn) { bool rejected=false; try { fn(); } catch (const std::exception&) {rejected=true;} Check(rejected,"expected failure"); }
void Put(const fs::path& p, const std::string& s) { fs::create_directories(p.parent_path()); std::ofstream o(p,std::ios::binary);o<<s; }
std::string Read(const fs::path& p) {std::ifstream f(p,std::ios::binary); return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
struct Temp {
 fs::path root=fs::temp_directory_path()/("pxwe-j115-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Temp(const fs::path& example) {fs::create_directories(root);fs::copy(example/"scripts",root/"scripts",fs::copy_options::recursive);}
 ~Temp() {std::error_code ec;fs::remove_all(root,ec);}
};
}
int main(int argc,char**argv) {
 if (argc<2) {std::cerr<<"example folder required\n"; return 1;}
 int passed=0;
 try {
  {
   Temp t(argv[1]); Project p; p.Load(t.root); Check(p.Validate().Ok(),"baseline invalid");
   p.SetAmmoOverrides("schema_smg","hitscan9",{{"fire_rate","990"},{"spread","0"},{"fire_sound","weapons/alternate.wav"}});
   p.PutVariant("schema_smg","alternate","view_model","models/v_alt.mdl","ammo:hitscan9");
   const auto prior = p.Find(Kind::Weapon,"schema_smg")->Serialize();
   const auto fingerprint=p.Validate().registry.Fingerprint();
   p.RenameId(Kind::Cartridge,"hitscan9","gold9");
   Check(p.Validate().Ok(),"cartridge rename did not validate");
   const auto* w=p.Find(Kind::Weapon,"schema_smg");
   Check(!p.Find(Kind::Cartridge,"hitscan9") && p.Find(Kind::Cartridge,"gold9"),"cartridge ID missing");
   Check(w->Get("default_cartridge")=="gold9"&&w->Get("ammo.gold9.spread")=="0"&&w->Get("ammo.gold9.fire_rate")=="990","default / ammo overrides not updated");
   Check(w->Get("ammo.gold9.fire_sound")=="weapons/alternate.wav"&&w->Get("ammo.hitscan9.fire_sound").empty(),"sound override not updated");
   Check(w->Get("asset.alternate.when")=="ammo:gold9","typed conditional reference not updated");
   Check(w->Get("ammo_name")=="schema_smg_ammo","unrelated ammo inventory ID incorrectly rewritten");
   Check(w->accepted.front().first=="gold9","accepted cartridge not updated");
   Check(p.Undo()&&p.Find(Kind::Cartridge,"hitscan9")&&p.Find(Kind::Weapon,"schema_smg")->Serialize()==prior,"rename wasn't one undo step");
   Check(p.Redo()&&p.Find(Kind::Cartridge,"gold9"),"rename redo failed");
   Check(p.Validate().registry.Fingerprint()!=fingerprint,"renamed registry should change fingerprint");
   p.Save(); Check(fs::exists(t.root/"scripts/cartridges/gold9.cartridge")&&!fs::exists(t.root/"scripts/cartridges/hitscan9.cartridge"),"renamed file not installed correctly");
   Check(Read(t.root/"scripts/cartridges/manifest.txt").find("gold9")!=std::string::npos,"manifest not updated");
   Project q; q.Load(t.root); Check(q.Validate().Ok()&&q.Find(Kind::Weapon,"schema_smg")->Get("ammo.gold9.spread")=="0","round trip lost override");
   ++passed; std::cout<<"PASS rename cartridge: typed links, zero spread, undo, redo, staged save, re-open\n";
  }
  {
   Temp t(argv[1]); Project p; p.Load(t.root);
   const auto before=*p.Find(Kind::Weapon,"schema_smg");
   p.RenameId(Kind::Weapon,"schema_smg","gold_smg");
   const auto* newW=p.Find(Kind::Weapon,"gold_smg");
   Check(newW && newW->Get("runtime_id")==before.Get("runtime_id"),"weapon runtime ID changed");
   Check(newW->Get("view_model")==before.Get("view_model")&&newW->Get("default_fire_sound")==before.Get("default_fire_sound"),"unrelated assets changed");
   p.Save(); Check(fs::exists(t.root/"scripts/weapons/gold_smg.weapon")&&!fs::exists(t.root/"scripts/weapons/schema_smg.weapon"),"weapon filename not changed");
   Project q; q.Load(t.root); Check(q.Validate().Ok()&&!q.Dirty(),"weapon rename cannot load");
   ++passed;std::cout<<"PASS rename weapon preserves runtime ID/model/sound and exports\n";
  }
  {
   Temp t(argv[1]); Project p; p.Load(t.root); const auto before=p.Find(Kind::Cartridge,"hitscan9")->Serialize();
   Reject([&]{p.RenameId(Kind::Cartridge,"hitscan9","energy9");});
   Reject([&]{p.RenameId(Kind::Cartridge,"hitscan9","ENERGY9");});
   Reject([&]{p.RenameId(Kind::Cartridge,"hitscan9","../bad");});
   Reject([&]{p.RenameId(Kind::Cartridge,"hitscan9","COM1");});
   Put(t.root/"scripts/cartridges/FOREIGN9.cartridge","untouched");
   Reject([&]{p.RenameId(Kind::Cartridge,"hitscan9","foreign9");});
   Check(p.Find(Kind::Cartridge,"hitscan9")->Serialize()==before&&!p.Dirty(),"rejection changed state");
   Check(Read(t.root/"scripts/cartridges/FOREIGN9.cartridge")=="untouched","external file overwritten");
   ++passed;std::cout<<"PASS collision/invalid input reject before any mutation\n";
  }
  {
   Temp t(argv[1]); Project p; p.Load(t.root); p.RenameId(Kind::Cartridge,"hitscan9","renamed9");
   Put(t.root/"scripts/cartridges/hitscan9.cartridge","externally changed");
   Reject([&]{p.Save();});
   Check(Read(t.root/"scripts/cartridges/hitscan9.cartridge")=="externally changed"&&!fs::exists(t.root/"scripts/cartridges/renamed9.cartridge"),"external conflict wrote assets");
   ++passed;std::cout<<"PASS external edits block rename save\n";
  }
  {
   for (const auto& field : {"id","attack_logic","damage_logic","default_damage","base_damage","damage_coefficient","legacy_bullet_type","default_fire_rate","default_spread","default_fire_sound","ammo.gold9.spread","asset.variant.when","view_model","runtime_id","mass_grams","reference_velocity_mps"}) {
    const auto text=DetailedPropertyHelp(std::string(field)=="default_fire_rate" || std::string(field)=="default_spread" || std::string(field)=="default_fire_sound" || std::string(field)=="runtime_id" || std::string(field)=="view_model" || std::string(field).rfind("ammo.",0)==0 || std::string(field).rfind("asset.",0)==0?Kind::Weapon:Kind::Cartridge,field);
    for (const auto& header:{"WHAT IS IT?","WHEN SHOULD I USE IT?","RELATED SETTINGS / IN-GAME RESULT","WORKED EXAMPLE","COMMON MISTAKE","SEE ALSO"})
     Check(text.find(header)!=std::string::npos,"help omitted required section");
   }
   ++passed;std::cout<<"PASS contextual help has meaning/when/links/example/warning/navigation\n";
  }
  std::cout<<passed<<"/5 passed\n";return 0;
 } catch(const std::exception& e) {std::cerr<<"FAIL J.11.5: "<<e.what()<<"\n";return 1;}
}
