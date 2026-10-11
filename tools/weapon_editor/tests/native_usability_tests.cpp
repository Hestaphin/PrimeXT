// Exercise the real native editor metadata, filesystem rules and history.
#include "pxwe_properties.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
using namespace pxwe;
int passed = 0, failed = 0;
void Check(bool v, const char* message) { if (!v) throw std::runtime_error(message); }
void Reject(const std::function<void()>& f) { bool rejected = false; try { f(); } catch (const std::exception&) { rejected = true; } Check(rejected, "Expected rejection"); }
void Test(const char* name, const std::function<void()>& f) { try { f(); ++passed; std::cout << "PASS " << name << '\n'; } catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; } }
struct Temp {
    fs::path path = fs::temp_directory_path() / ("pxwe-usability-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() { Check(fs::create_directory(path), "Fixture creation"); }
    ~Temp() { std::error_code e; fs::remove_all(path, e); }
};
void Put(const fs::path& path, const std::string& text = "resource") { fs::create_directories(path.parent_path()); std::ofstream o(path, std::ios::binary); o << text; Check(bool(o), "Fixture write"); }
Project New(const fs::path& root) { Project p; p.Load(root); p.Add(Kind::Cartridge, "fmj"); p.Add(Kind::Weapon, "pistol"); return p; }
}
int main(int argc, char** argv) {
    Test("all supported flat properties have specific help and sections", [] {
        for (const auto kind : {Kind::Weapon, Kind::Cartridge}) for (const auto& key : FieldNames(kind)) {
            const auto p = DescribeProperty(kind, key);
            Check(p.section != "Other fields" && p.description.size() > 25, ("Missing specific help: " + key).c_str());
        }
    });
    Test("IDs remain read-only while version3 is selectable", [] {
        Check(DescribeProperty(Kind::Weapon, "id").readOnly, "ID edit allowed");
        const auto p = DescribeProperty(Kind::Weapon, "version");
        Check(std::find(p.choices.begin(), p.choices.end(), "3") != p.choices.end(), "Version3 missing");
        Check(!DescribeProperty(Kind::Cartridge, "damage_coefficient").readOnly, "Coefficient not editable");
    });
    Test("every action field has primary and secondary help", [] {
        for (const auto& field : {"behavior", "mode", "interval", "spread", "projectiles", "ammo_cost", "burst_count", "recoil_pitch", "recoil_yaw", "animation", "sound", "event"}) {
            const auto a = DescribeProperty(Kind::Weapon, "primary_" + std::string(field));
            const auto b = DescribeProperty(Kind::Weapon, "secondary_" + std::string(field));
            Check(a.section == "Primary attack" && b.section == "Secondary attack" && a.description == b.description, "Action metadata mismatch");
        }
    });
    Test("conditional role controls path browsing and when choices", [] {
        auto a = Parse("id=pistol\nasset.a.role=fire_sound\nasset.a.path=weapons/a.wav\nasset.a.when=random\n", Kind::Weapon);
        Check(DescribeProperty(a.kind, "asset.a.path", &a).resource == ResourceType::Sound, "Conditional sound browse");
        a.Set("asset.a.role", "view_model"); Check(DescribeProperty(a.kind, "asset.a.path", &a).resource == ResourceType::Model, "Conditional model browse");
        a.Set("asset.a.role", "kill_sprite"); Check(DescribeProperty(a.kind, "asset.a.path", &a).resource == ResourceType::KillSprite, "Kill browse");
        const auto w = DescribeProperty(a.kind, "asset.a.when", &a);
        Check(w.section == "Conditional assets" && w.choices.size() == 6, "When help");
        a.Set("asset.a.role", "bad"); Check(DescribeProperty(a.kind, "asset.a.path", &a).resource == ResourceType::None, "Invalid role browsable");
    });
    Test("all ammunition override roles have help and resource types", [] {
        for (const auto& role : {"view_model", "player_model", "world_model", "fire_sound", "reload_sound", "sprite", "hud_sprite", "kill_sprite"}) {
            const auto p = DescribeProperty(Kind::Weapon, "ammo.fmj." + std::string(role));
            Check(p.section == "Ammunition overrides" && p.resource != ResourceType::None, "Override metadata missing");
        }
    });
    Test("unsupported keys stay visible and explain repair", [] {
        auto a = Parse("id=x\nunsupported=bad\n", Kind::Cartridge);
        const auto p = DescribeProperty(a.kind, "unsupported", &a);
        Check(p.section == "Other fields" && p.description.find("clear") != std::string::npos, "Unknown repair help");
        const auto rows = PropertyRows(a, {});
        Check(std::any_of(rows.begin(), rows.end(), [](const auto& r) { return r.key == "unsupported"; }), "Unknown hidden");
    });
    Test("section collapse preserves data and expands without duplicate rows", [] {
        const auto a = Parse("id=x\nversion=3\nasset.a.role=view_model\nasset.a.path=models/v.mdl\nasset.a.when=default\nammo.fmj.sprite=sprites/fmj.spr\n", Kind::Weapon);
        const auto before = a.Serialize(); const auto full = PropertyRows(a, {});
        const auto hidden = PropertyRows(a, {"Models and animations", "Conditional assets"});
        Check(hidden.size() < full.size() && a.Serialize() == before, "Collapse changed data");
        std::set<std::string> keys;
        for (const auto& r : full) if (!r.Heading()) Check(keys.insert(r.key).second, "Duplicate row");
        for (const auto& r : hidden) if (r.section == "Conditional assets" || r.section == "Models and animations") Check(r.Heading(), "Collapsed property visible");
        Check(PropertyRows(a, {}).size() == full.size(), "Expand lost fields");
    });
    Test("new weapons generate all three ID-based model paths", [] {
        Temp t; auto p = New(t.path); p.Add(Kind::Weapon, "My_Gun-2");
        const auto* w = p.Find(Kind::Weapon, "My_Gun-2");
        Check(w->Get("view_model") == "models/v_My_Gun-2.mdl" && w->Get("player_model") == "models/p_My_Gun-2.mdl" && w->Get("world_model") == "models/w_My_Gun-2.mdl", "Generated paths wrong");
        Check(p.Validate().Ok(), "Generated definitions invalid");
    });
    Test("duplicating preserves authored model paths and allocates fresh runtime ID", [] {
        Temp t; auto p = New(t.path); p.SetField(Kind::Weapon, "pistol", "view_model", "models/authored.mdl"); p.Add(Kind::Weapon, "copy", "pistol");
        const auto* w = p.Find(Kind::Weapon, "copy");
        Check(w->Get("view_model") == "models/authored.mdl" && w->Get("runtime_id") == "33", "Duplicate changed authored paths");
    });
    Test("default paths undo redo save and reload", [] {
        Temp t; auto p = New(t.path); p.Save(); p.Add(Kind::Weapon, "next");
        Check(p.Undo() && !p.Find(Kind::Weapon, "next") && p.Redo(), "Creation history");
        p.Save(); Project q; q.Load(t.path); Check(q.Find(Kind::Weapon, "next")->Get("world_model") == "models/w_next.mdl" && q.Validate().Ok(), "Generated path round trip");
    });
    Test("duplicate ID checks reject Windows case aliases without mutation", [] {
        Temp t; auto p = New(t.path); p.Save();
        Reject([&] { p.CheckNewId(Kind::Weapon, "PISTOL"); }); Reject([&] { p.Add(Kind::Cartridge, "FMJ"); });
        Check(!p.Dirty() && p.Data().weapons.size() == 1, "Collision mutated project");
    });
    Test("unlisted disk files and directories reject creation immediately", [] {
        Temp t; auto p = New(t.path); p.Save(); Put(t.path / "scripts/weapons/Other.weapon", "untouched");
        Reject([&] { p.Add(Kind::Weapon, "other", "pistol"); });
        fs::create_directory(t.path / "scripts/cartridges/blocked.cartridge"); Reject([&] { p.Add(Kind::Cartridge, "blocked"); });
        Check(!p.Dirty() && !p.Find(Kind::Weapon, "other"), "Unlisted collision mutation");
    });
    Test("deleted owned ID can be recreated and saved with exact spelling", [] {
        Temp t; auto p = New(t.path); p.Save(); p.Remove(Kind::Weapon, "pistol");
        Reject([&] { p.Add(Kind::Weapon, "PISTOL"); }); p.Add(Kind::Weapon, "pistol"); p.Save();
        Check(p.Validate().Ok(), "Owned recreation failed");
    });
    Test("creation requires a folder and valid ID", [] {
        Project p; Reject([&] { p.Add(Kind::Weapon, "gun"); }); Temp t; p.Load(t.path);
        for (const auto& id : {"", "../escape", "has space", "C:gun"}) Reject([&] { p.CheckNewId(Kind::Weapon, id); });
        Check(p.Data().weapons.empty(), "Invalid creation mutated project");
    });
    Test("model sprite and event browsing writes portable relative paths", [] {
        Temp t;
        for (const auto& item : std::vector<std::pair<std::string, ResourceType>>{{"models/v_Gun.MDL", ResourceType::Model}, {"sprites/gun.spr", ResourceType::Sprite}, {"sprites/gun.txt", ResourceType::Sprite}, {"sprites/kill.spr", ResourceType::KillSprite}, {"events/fire.sc", ResourceType::Event}}) {
            Put(t.path / fs::u8path(item.first)); Check(ResourcePath(t.path, t.path / fs::u8path(item.first), item.second) == item.first, "Relative browse path wrong");
        }
    });
    Test("sound browsing strips the disk sound prefix exactly once", [] {
        Temp t; Put(t.path / "sound/weapons/fire.wav"); Put(t.path / "sound/sound/nested.wav");
        Check(ResourcePath(t.path, t.path / "sound/weapons/fire.wav", ResourceType::Sound) == "weapons/fire.wav", "Sound prefix not removed");
        Check(ResourcePath(t.path, t.path / "sound/sound/nested.wav", ResourceType::Sound) == "sound/nested.wav", "Sound prefix removed twice");
        Put(t.path / "weapons/outside.wav"); Reject([&] { ResourcePath(t.path, t.path / "weapons/outside.wav", ResourceType::Sound); });
    });
    Test("browsing rejects outside game paths including prefix lookalikes", [] {
        Temp t; fs::create_directory(t.path / "game"); Put(t.path / "game-other/test.mdl");
        Reject([&] { ResourcePath(t.path / "game", t.path / "game-other/test.mdl", ResourceType::Model); });
        Temp outside; Put(outside.path / "test.mdl"); Reject([&] { ResourcePath(t.path, outside.path / "test.mdl", ResourceType::Model); });
    });
    Test("browsing rejects wrong extensions missing files and unportable names", [] {
        Temp t;
        for (const auto& file : {"models/bad name.mdl", "models/bad..name.mdl", "models/sound.wav", "models/\xc3\xa9.mdl"}) {
            Put(t.path / fs::u8path(file)); Reject([&] { ResourcePath(t.path, t.path / fs::u8path(file), ResourceType::Model); });
        }
        Put(t.path / "sprites/list.txt"); Reject([&] { ResourcePath(t.path, t.path / "sprites/list.txt", ResourceType::KillSprite); });
        Reject([&] { ResourcePath(t.path, t.path / "missing.mdl", ResourceType::Model); });
        Reject([&] { ResourcePath(t.path, t.path / "models", ResourceType::Model); });
        Reject([&] { ResourcePath(t.path, t.path / "sprites/list.txt", ResourceType::None); });
    });
    Test("symlink resources cannot escape the game", [] {
        Temp t, outside; Put(outside.path / "test.mdl"); std::error_code e; fs::create_symlink(outside.path / "test.mdl", t.path / "link.mdl", e);
        if (e) { std::cout << "  symlink unavailable: " << e.message() << '\n'; return; }
        Reject([&] { ResourcePath(t.path, t.path / "link.mdl", ResourceType::Model); });
    });
    Test("browse values use normal undo redo and validation", [] {
        Temp t; auto p = New(t.path); p.Save(); Put(t.path / "models/new.mdl");
        const auto rel = ResourcePath(t.path, t.path / "models/new.mdl", ResourceType::Model);
        p.SetField(Kind::Weapon, "pistol", "view_model", rel);
        Check(p.Undo() && p.Find(Kind::Weapon, "pistol")->Get("view_model") == "models/v_pistol.mdl", "Browse undo failed");
        Check(p.Redo() && p.Validate().Ok(), "Browse redo invalid"); p.Save();
        Project q; q.Load(t.path); Check(q.Find(Kind::Weapon, "pistol")->Get("view_model") == rel, "Browse save changed path");
    });
    if (argc == 2) Test("existing runtime pack retains fingerprints and all conditions", [&] {
        const auto source = fs::path(argv[1]) / "documentation/examples/modular-runtime9_4";
        Temp t; fs::copy(source, t.path, fs::copy_options::recursive); Project p; p.Load(t.path);
        Check(p.Validate().Ok(), "Runtime sample invalid");
        const auto before = p.Validate().registry.RuntimeFingerprint(); const auto content = p.Validate().registry.Fingerprint();
        p.SetField(Kind::Weapon, "test_pistol", "display_name", "Edited name"); Check(p.Undo(), "Undo edit");
        p.Save(); Project q; q.Load(t.path);
        Check(q.Validate().registry.RuntimeFingerprint() == before && q.Validate().registry.Fingerprint() == content, "Editor changed existing semantics");
        const auto rows = PropertyRows(*q.Find(Kind::Weapon, "test_pistol"), {});
        Check(std::any_of(rows.begin(), rows.end(), [](const auto& r) { return r.section == "Conditional assets" && !r.Heading(); }), "Existing conditions hidden");
    });
    std::cout << passed << " passed, " << failed << " failed\n"; return failed ? 1 : 0;
}
