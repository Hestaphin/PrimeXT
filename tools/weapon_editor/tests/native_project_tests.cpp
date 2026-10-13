// C++ backend integration tests; exercise actual files and the game Registry.
#include "pxwe_project.h"
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#ifdef __linux__
#include <linux/capability.h>
#include <sys/syscall.h>
#endif
namespace {
using namespace pxwe;
int passed = 0, failed = 0;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void Reject(const std::function<void()>& action) {
    bool rejected = false; try { action(); } catch (const std::exception&) { rejected = true; }
    Check(rejected, "Expected operation to reject invalid input.");
}
struct Temp {
    fs::path path;
    Temp() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int i = 0; i < 100; ++i) {
            path = fs::temp_directory_path() / ("pxwe-test-" + std::to_string(stamp) + "-" + std::to_string(i));
            if (fs::create_directory(path)) return;
        }
        throw std::runtime_error("Cannot create test directory.");
    }
    ~Temp() { std::error_code ec; fs::permissions(path, fs::perms::owner_all, ec); fs::remove_all(path, ec); }
};
void Put(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path()); std::ofstream out(path, std::ios::binary); out << text;
    out.close(); Check(bool(out), "Fixture write failed.");
}
std::string Get(const fs::path& path) {
    std::ifstream in(path, std::ios::binary); std::ostringstream out; out << in.rdbuf();
    Check(bool(in), "Fixture read failed."); return out.str();
}
std::map<std::string, std::string> Disk(const fs::path& root) {
    std::map<std::string, std::string> files;
    for (const auto& e : fs::recursive_directory_iterator(root))
        if (e.is_regular_file()) files[fs::relative(e.path(), root).u8string()] = Get(e.path());
    return files;
}
Project ValidProject(const fs::path& root) {
    Project p; p.Load(root); p.AddLegacy(Kind::Cartridge, "fmj"); p.AddLegacy(Kind::Weapon, "pistol");
    Check(p.Validate().Ok(), "Default project was invalid."); return p;
}
void Test(const char* name, const std::function<void()>& action) {
    try { action(); ++passed; std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
}
void U32(std::string& data, size_t at, uint32_t value) {
    for (int i = 0; i < 4; ++i) data.at(at + i) = static_cast<char>(value >> (8 * i));
}
void F32(std::string& data, size_t at, float value) {
    uint32_t bits = 0; std::memcpy(&bits, &value, sizeof(bits)); U32(data, at, bits);
}
std::string Model() {
    std::string data(244 + 176, '\0'); data.replace(0, 4, "IDST"); U32(data, 4, 10); data.replace(8, 4, "test");
    U32(data, 72, static_cast<uint32_t>(data.size()));
    for (int i = 0; i < 3; ++i) { F32(data, 88 + i * 4, -10); F32(data, 100 + i * 4, 20); }
    U32(data, 164, 1); U32(data, 168, 244); data.replace(244, 4, "idle"); F32(data, 244 + 32, 30); U32(data, 244 + 56, 12);
    return data;
}
} // namespace

int main(int argc, char** argv) {
    Test("source-free folder discovery ignores working directory", [] {
        Temp t; const auto game = t.path / fs::u8path(u8"PrimeXT 游戏"); fs::create_directories(game / "devkit");
        Check(DiscoverRoot(game / "devkit/pxwe.exe") == fs::weakly_canonical(game), "devkit root");
        Check(DiscoverRoot(game / "pxwe.exe") == fs::weakly_canonical(game), "direct game root");
        Check(DiscoverRoot(game / "bin/pxwe.exe") == fs::weakly_canonical(game), "bin root");
        Check(DiscoverRoot(game / "devkit/pxwe.exe", t.path) == fs::weakly_canonical(t.path), "explicit root");
    });
    Test("draft weapon can be created before its ammunition", [] {
        Temp t; Project p; p.Load(t.path); p.AddLegacy(Kind::Weapon, "draft"); Check(!p.Validate().Ok(), "Draft unexpectedly valid");
        p.AddLegacy(Kind::Cartridge, "fmj"); p.Allow("draft", "fmj", "360"); p.SetDefault("draft", "fmj");
        Check(p.Validate().Ok(), "Draft could not be completed.");
    });
    Test("native templates validate and round-trip through real registry", [] {
        Temp t; auto p = ValidProject(t.path); p.Save(); Check(!p.Dirty(), "Saved project is dirty.");
        Project loaded; loaded.Load(t.path);
        Check(loaded.Validate().registry.Fingerprint() == p.Validate().registry.Fingerprint(), "Fingerprint changed.");
        Check(loaded.Find(Kind::Weapon, "pistol")->Get("primary_behavior") == "ballistic", "Attack template");
        Check(loaded.Validate().registry.FindCartridge("fmj")->schemaVersion == 2, "Cartridge template");
        for (const auto& e : fs::directory_iterator(t.path)) Check(e.path().filename().u8string().find(".pxwe-stage-") != 0, "Stage leaked.");
    });
    Test("undo redo include fields compatibility and default changes", [] {
        Temp t; auto p = ValidProject(t.path); p.Save(); p.SetField(Kind::Weapon, "pistol", "clip_size", "18");
        Check(p.Dirty() && p.Undo() && !p.Dirty(), "Undo did not restore saved state.");
        Check(p.Redo() && p.Find(Kind::Weapon, "pistol")->Get("clip_size") == "18", "Redo field");
        p.AddLegacy(Kind::Cartridge, "jhp"); p.Allow("pistol", "jhp", "300"); p.SetDefault("pistol", "jhp");
        Check(p.Find(Kind::Weapon, "pistol")->Get("ammo_name") == "jhp", "Default ammo name not synchronized.");
        Check(p.Undo() && p.Find(Kind::Weapon, "pistol")->Get("default_cartridge") == "fmj", "Undo default");
        Check(p.Redo() && p.Validate().Ok(), "Redo default");
        p.Undo(); p.SetField(Kind::Weapon, "pistol", "clip_size", "20"); Check(!p.Redo(), "New edit retained redo.");
    });
    Test("one cartridge supports different velocities in different weapons", [] {
        Temp t; auto p = ValidProject(t.path); p.AddLegacy(Kind::Weapon, "rifle", "pistol"); p.Allow("rifle", "fmj", "410");
        const auto v = p.Validate(); Check(v.Ok(), "Compatibility invalid");
        Check(v.registry.FindWeapon("pistol")->compatible[0].muzzleVelocityMps == 360, "Original velocity changed.");
        Check(v.registry.FindWeapon("rifle")->compatible[0].muzzleVelocityMps == 410, "New velocity missing.");
    });
    Test("duplicate weapons get fresh runtime IDs including alternate spellings", [] {
        Temp t; auto p = ValidProject(t.path); p.SetField(Kind::Weapon, "pistol", "runtime_id", "+032");
        p.AddLegacy(Kind::Weapon, "copy", "pistol");
        Check(p.Find(Kind::Weapon, "copy")->Get("runtime_id") == "33", "Duplicate runtime ID.");
        Check(p.Validate().Ok(), "Duplicate did not validate.");
    });
    Test("runtime weapon ID exhaustion is rejected before mutation", [] {
        Temp t; auto p = ValidProject(t.path);
        for (int i = 33; i <= 62; ++i) p.AddLegacy(Kind::Weapon, "gun_" + std::to_string(i), "pistol");
        Reject([&] { p.AddLegacy(Kind::Weapon, "overflow"); }); Check(p.Data().weapons.size() == 31, "Overflow mutated project.");
        Check(p.Validate().Ok(), "31 weapons rejected.");
    });
    Test("case-insensitive IDs and invalid identifiers are rejected", [] {
        Temp t; auto p = ValidProject(t.path);
        for (const std::string id : {"FMJ", "../fmj", "", "bad id", "bad:id"})
            Reject([&] { p.AddLegacy(Kind::Cartridge, id); });
        Reject([&] { p.AddLegacy(Kind::Weapon, "PISTOL"); });
        Reject([&] { p.SetField(Kind::Weapon, "pistol", "id", "renamed"); });
        Reject([&] { p.SetField(Kind::Weapon, "pistol", "display_name", "name\nruntime_id = 44"); });
        Reject([&] { p.SetField(Kind::Weapon, "pistol", "display_name", std::string("a\0b", 3)); });
    });
    Test("parser rejects duplicate keys accepted rows NULs and oversized files", [] {
        Reject([] { Parse("id=x\nid=y\n", Kind::Cartridge); });
        Reject([] { Parse("id=w\naccept=fmj:300\naccept=FMJ:400\n", Kind::Weapon); });
        Reject([] { Parse(std::string("id=a\0b\n", 7), Kind::Cartridge); });
        Reject([] { Parse(std::string(65537, 'x'), Kind::Weapon); });
        Reject([] { Parse("id=w\naccept=fmj\n", Kind::Weapon); });
        const auto a = Parse("# comment\r\nid = w\r\n; ignored\n", Kind::Weapon); Check(a.Id() == "w", "CRLF parse");
    });
    Test("semantic invalid edits cannot write and remain repairable", [] {
        Temp t; auto p = ValidProject(t.path); p.Save(); const auto before = Disk(t.path);
        for (const auto& field : std::vector<std::pair<std::string, std::string>>{
            {"clip_size", "0"}, {"primary_behavior", "unknown"}, {"view_model", "../outside.mdl"},
            {"primary_recoil_pitch", "nan"}, {"muzzle_flash", "maybe"}}) {
            const auto old = p.Find(Kind::Weapon, "pistol")->Get(field.first);
            p.SetField(Kind::Weapon, "pistol", field.first, field.second); Check(!p.Validate().Ok(), "Invalid edit accepted.");
            Reject([&] { p.Save(); }); Check(Disk(t.path) == before, "Invalid save wrote files.");
            p.SetField(Kind::Weapon, "pistol", field.first, old);
        }
        Check(p.Validate().Ok(), "Repair failed.");
    });
    Test("invalid values already on disk can be opened and repaired", [] {
        Temp t; auto p = ValidProject(t.path); p.Save();
        const auto file = t.path / "scripts/weapons/pistol.weapon"; auto text = Get(file);
        const auto pos = text.find("clip_size = 15"); Check(pos != std::string::npos, "Fixture clip");
        text.replace(pos, 14, "clip_size = 0"); Put(file, text);
        Project loaded; loaded.Load(t.path); Check(!loaded.Validate().Ok(), "Invalid loaded project accepted.");
        loaded.SetField(Kind::Weapon, "pistol", "clip_size", "20"); loaded.Save(); Check(loaded.Validate().Ok(), "Repair save failed.");
    });
    Test("compatibility protects used/default cartridges and checks velocities", [] {
        Temp t; auto p = ValidProject(t.path);
        Reject([&] { p.Remove(Kind::Cartridge, "fmj"); }); Reject([&] { p.Disallow("pistol", "fmj"); });
        Reject([&] { p.Allow("pistol", "missing", "300"); });
        for (const std::string v : {"nan", "inf", "0", "10001", "300oops", "300\naccept=x:2"})
            Reject([&] { p.Allow("pistol", "fmj", v); });
        p.AddLegacy(Kind::Cartridge, "jhp"); Reject([&] { p.SetDefault("pistol", "jhp"); });
        p.Allow("pistol", "jhp", "310"); p.SetDefault("pistol", "jhp"); p.Disallow("pistol", "fmj");
        p.Remove(Kind::Cartridge, "fmj"); Check(p.Validate().Ok(), "Compatibility replacement failed.");
    });
    Test("save preserves unrelated and unlisted files and only removes owned files", [] {
        Temp t; auto p = ValidProject(t.path); p.AddLegacy(Kind::Cartridge, "unused"); p.Save();
        Put(t.path / "scripts/weapons/unlisted.weapon", "unrelated\n"); Put(t.path / "config.cfg", "bind a b\n");
        p.Remove(Kind::Cartridge, "unused"); p.SetField(Kind::Weapon, "pistol", "clip_size", "18"); p.Save();
        Check(!fs::exists(t.path / "scripts/cartridges/unused.cartridge"), "Owned deletion missing.");
        Check(Get(t.path / "scripts/weapons/unlisted.weapon") == "unrelated\n", "Unlisted file changed.");
        Check(Get(t.path / "config.cfg") == "bind a b\n", "Unrelated file changed.");
        const auto before = Disk(t.path);
        Reject([&] { p.AddLegacy(Kind::Weapon, "unlisted", "pistol"); });
        Check(Disk(t.path) == before && !p.Find(Kind::Weapon, "unlisted"), "Unlisted definition collision changed data.");
    });
    Test("external file or manifest changes block save without touching files", [] {
        Temp t; auto p = ValidProject(t.path); p.Save(); p.SetField(Kind::Weapon, "pistol", "clip_size", "20");
        const auto path = t.path / "scripts/cartridges/fmj.cartridge"; const auto text = Get(path);
        Put(path, text + "# external\n"); auto before = Disk(t.path);
        Reject([&] { p.Save(); }); Check(Disk(t.path) == before, "External asset overwritten.");
        Put(path, text); Put(t.path / "scripts/weapons/manifest.txt", "pistol\n# external\n"); before = Disk(t.path);
        Reject([&] { p.Save(); }); Check(Disk(t.path) == before, "External manifest overwritten.");
    });
    Test("structural load failure leaves current folder and edits untouched", [] {
        Temp t, bad; auto p = ValidProject(t.path); const auto fingerprint = p.Validate().registry.Fingerprint();
        Put(bad.path / "scripts/weapons/manifest.txt", "../outside\n"); Reject([&] { p.Load(bad.path); });
        Check(p.Root() == fs::weakly_canonical(t.path) && p.Validate().registry.Fingerprint() == fingerprint, "Failed load mutated project.");
        Put(bad.path / "scripts/cartridges/manifest.txt", "fmj\nFMJ\n"); Reject([&] { p.Load(bad.path); });
        Put(bad.path / "scripts/cartridges/manifest.txt", "missing\n"); Reject([&] { p.Load(bad.path); });
    });
    Test("linked script folders cannot escape the selected game folder", [] {
        Temp t, outside; std::error_code ec; fs::create_directory_symlink(outside.path, t.path / "scripts", ec);
        // Windows without Developer Mode may not allow symlink creation.
        if (ec) { std::cout << "  symlink unavailable: " << ec.message() << '\n'; return; }
        Project p; Reject([&] { p.Load(t.path); }); Check(Disk(outside.path).empty(), "Linked folder was changed.");
    });
#ifndef _WIN32
    Test("mid-export I/O failure restores every original file", [] {
        Temp t; auto p = ValidProject(t.path); p.Save(); const auto before = Disk(t.path);
        p.SetField(Kind::Weapon, "pistol", "clip_size", "20");
        // Save backs up cartridge files before reaching weapons. Remove write
        // access to the latter to fail after the first backups have moved.
        fs::permissions(t.path, fs::perms::all);
        fs::permissions(t.path / "scripts", fs::perms::all);
        fs::permissions(t.path / "scripts/cartridges", fs::perms::all);
        fs::permissions(t.path / "scripts/weapons", fs::perms::owner_read | fs::perms::owner_exec |
            fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec);
        pid_t child = fork(); Check(child >= 0, "fork failed");
        if (child == 0) {
#ifdef __linux__
            if (geteuid() == 0) {
                // Keep ownership/access to private ancestor folders, but stop
                // bypassing the write permissions which simulate the failure.
                __user_cap_header_struct header{_LINUX_CAPABILITY_VERSION_3, 0};
                __user_cap_data_struct caps[2]{};
                if (syscall(SYS_capget, &header, caps) != 0) _exit(3);
                caps[0].effective &= ~((1u << CAP_DAC_OVERRIDE) | (1u << CAP_DAC_READ_SEARCH));
                if (syscall(SYS_capset, &header, caps) != 0) _exit(3);
            }
#else
            if (geteuid() == 0 && (setgid(65534) != 0 || setuid(65534) != 0)) _exit(3);
#endif
            try { p.Save(); _exit(4); }
            catch (const std::exception& e) {
                _exit(std::string(e.what()).find("Original files restored.") != std::string::npos ? 0 : 5);
            }
        }
        int status = 0; Check(waitpid(child, &status, 0) == child, "wait failed");
        fs::permissions(t.path / "scripts/weapons", fs::perms::owner_all);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            throw std::runtime_error("I/O failure did not roll back. Child status: " + std::to_string(status));
        Check(Disk(t.path) == before, "Rollback bytes differ.");
    });
#endif
    Test("trajectory uses actual game integrator and invalid inputs are rejected", [] {
        modular::Cartridge c; c.massGrams = 8; c.diameterMm = 9; c.dragCoefficient = 0;
        const auto noDrag = Flight(c, 300); Check(noDrag.size() == 41, "Preview sampling changed.");
        Check(std::abs(noDrag.back().position.x - 600) < 1e-8, "Horizontal no-drag flight.");
        Check(std::abs(noDrag.back().position.z + .5 * 9.80665 * 4) < 1e-8, "No-drag gravity.");
        c.dragCoefficient = .3f; const auto drag = Flight(c, 300);
        Check(drag.back().position.x < 600 && drag.back().velocity.x > 0, "Drag not applied.");
        modular::BallisticParameters params; params.massKg = c.massGrams / 1000.0; params.diameterM = c.diameterMm / 1000.0;
        params.dragCoefficient = c.dragCoefficient; modular::BallisticState real; real.velocity.x = 300;
        for (int i = 0; i < 400; ++i) real = modular::IntegrateBallistic(real, params, .005);
        Check(drag.back().position.x == real.position.x && drag.back().position.z == real.position.z, "Preview differs from game.");
        Reject([&] { Flight(c, std::numeric_limits<double>::quiet_NaN()); });
        c.massGrams = 0; Reject([&] { Flight(c, 300); });
    });
    Test("Studio v10 metadata and malformed length bounds table handling", [] {
        Temp t; const auto file = t.path / "test.mdl"; const auto good = Model(); Put(file, good);
        const auto info = ReadModel(file);
        Check(info.name == "test" && info.mins[0] == -10 && info.maxs[2] == 20, "Model metadata wrong.");
        Check(info.sequences.size() == 1 && info.sequences[0].name == "idle" && info.sequences[0].frames == 12, "Sequence metadata wrong.");
        for (int variant = 0; variant < 7; ++variant) {
            auto bad = good;
            if (variant == 0) U32(bad, 4, 11);
            if (variant == 1) U32(bad, 72, 999999);
            if (variant == 2) U32(bad, 164, 2049);
            if (variant == 3) U32(bad, 168, 0xfffffff0);
            if (variant == 4) F32(bad, 88, std::numeric_limits<float>::quiet_NaN());
            if (variant == 5) F32(bad, 88, 100);
            if (variant == 6) bad.resize(100);
            Put(file, bad); Reject([&] { ReadModel(file); });
        }
    });
    Test("unknown optional fields can be removed and native parser catches ammo limits", [] {
        Temp t; auto p = ValidProject(t.path); p.SetField(Kind::Weapon, "pistol", "unknown", "1");
        Check(!p.Validate().Ok(), "Unknown field accepted."); p.SetField(Kind::Weapon, "pistol", "unknown", "");
        Check(p.Validate().Ok(), "Unknown field removal failed.");
        for (int i = 1; i <= 31; ++i) p.AddLegacy(Kind::Cartridge, "ammo_" + std::to_string(i), "fmj");
        Check(!p.Validate().Ok(), "32 cartridge slot overflow accepted.");
    });
    if (argc == 2) for (const std::string example : {"modular-weapon", "modular-ballistics", "modular-legacy-profiles"}) {
        Test(("existing example round-trip " + example).c_str(), [&] {
            Temp t; const auto source = fs::u8path(argv[1]) / "documentation/examples" / example;
            fs::copy(source, t.path, fs::copy_options::recursive); Project p; p.Load(t.path);
            const auto v = p.Validate(); Check(v.Ok(), "Existing example rejected."); p.Save();
            Project q; q.Load(t.path); Check(q.Validate().registry.Fingerprint() == v.registry.Fingerprint(), "Example fingerprint changed.");
        });
    }
    std::cout << passed << " passed, " << failed << " failed\n"; return failed ? 1 : 0;
}
