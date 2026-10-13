// Copyright (C) 2026 PrimeXT contributors. GPL-3.0-or-later.
#include "pxwe_project.h"
#include "pxwe_preview.h"
#include "cartridge_damage.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace pxwe {
namespace {
constexpr size_t kMaxFile = 65536;
constexpr const char* kCategoryFile = "scripts/editor_categories.txt";
std::string Trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    return begin == std::string::npos ? "" : s.substr(begin, s.find_last_not_of(" \t\r\n") - begin + 1);
}
std::string Lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return s;
}
void SingleLine(const std::string& s) {
    if (s.find_first_of("\r\n") != std::string::npos || s.find('\0') != std::string::npos)
        throw std::runtime_error("A definition value must be a single line without NUL bytes.");
}
std::vector<Asset>& Collection(State& s, Kind kind) {
    return kind == Kind::Weapon ? s.weapons : s.cartridges;
}
const std::vector<Asset>& Collection(const State& s, Kind kind) {
    return kind == Kind::Weapon ? s.weapons : s.cartridges;
}
bool InCategory(const std::string& path, const std::string& parent) {
    return path == parent || (path.size() > parent.size() &&
        path.compare(0, parent.size(), parent) == 0 && path[parent.size()] == '/');
}
std::vector<std::string> CategoryPaths(const State& state, Kind kind) {
    std::set<std::string> paths;
    auto include = [&](std::string path) {
        if (!ValidCategory(path)) return; // Invalid drafts remain editable at the root.
        while (!path.empty()) { paths.insert(path); path = CategoryParent(path); }
    };
    for (const auto& c : state.categories) if (c.kind == kind) include(c.path);
    for (const auto& a : Collection(state, kind)) include(a.Get("category"));
    return {paths.begin(), paths.end()};
}
std::string CategoryText(const std::vector<Category>& categories) {
    if (categories.empty()) return {};
    if (categories.size() > 256) throw std::runtime_error("At most 256 explicit editor categories are supported.");
    std::set<std::pair<Kind, std::string>> sorted;
    for (const auto& c : categories) {
        if (!ValidCategory(c.path) || !sorted.emplace(c.kind, c.path).second)
            throw std::runtime_error("Invalid or duplicate editor category.");
    }
    std::string text = "# pxwe empty/category folders; the game reads asset category fields.\nversion = 1\n";
    for (const auto& c : sorted) text += (c.first == Kind::Weapon ? "weapon = " : "cartridge = ") + c.second + "\n";
    if (text.size() > kMaxFile) throw std::runtime_error("Editor categories exceed 64 KiB.");
    return text;
}
std::vector<Category> ParseCategories(const std::string& text) {
    if (text.find('\0') != std::string::npos) throw std::runtime_error("NUL in editor categories.");
    std::vector<Category> result; std::set<std::pair<Kind, std::string>> seen;
    std::istringstream in(text); std::string line; bool version = false;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const auto equal = line.find('=');
        if (equal == std::string::npos) throw std::runtime_error("Editor categories need key = value.");
        const auto key = Trim(line.substr(0, equal)), value = Trim(line.substr(equal+1));
        if (key == "version") {
            if (version || value != "1") throw std::runtime_error("Expected editor category version 1 once.");
            version = true; continue;
        }
        if (key != "weapon" && key != "cartridge") throw std::runtime_error("Unknown editor category property: " + key);
        const Kind kind = key == "weapon" ? Kind::Weapon : Kind::Cartridge;
        if (!ValidCategory(value) || !seen.emplace(kind, value).second)
            throw std::runtime_error("Invalid or duplicate editor category: " + value);
        result.push_back({kind, value});
        if (result.size() > 256) throw std::runtime_error("Too many editor categories.");
    }
    if (!version) throw std::runtime_error("Editor category version is missing.");
    return result;
}
void AssignCategory(Asset& asset, const std::string& path) {
    if (!ValidCategory(path, true)) throw std::runtime_error("Category paths use 1..16 ASCII ID segments, up to 255 characters. Blank is the root.");
    if (!path.empty() && asset.Get("version") != "3" && asset.Get("version") != "4") {
        if (asset.kind == Kind::Cartridge) {
            // Convert with the same parser/default damage as the game, including
            // version 1. The assignment and conversion share one transaction.
            Asset probe=asset; probe.Set("category","");
            modular::Registry registry;
            registry.Load([&](const std::string& rel, std::string& data) {
                if (rel == "scripts/cartridges/manifest.txt") { data = asset.Id()+"\n"; return true; }
                if (rel == "scripts/weapons/manifest.txt") { data.clear(); return true; }
                if (rel == "scripts/cartridges/"+asset.Id()+".cartridge") { data = probe.Serialize(); return true; }
                return false;
            });
            const auto* c = registry.FindCartridge(asset.Id());
            if (!registry.Valid() || !c) throw std::runtime_error("Repair this cartridge before assigning a category.");
            const double k = c->schemaVersion>=3 ? c->damageCoefficient :
                c->damage * modular::ProjectileAreaMm2(c->diameterMm) /
                modular::KineticEnergyJ(c->massGrams, c->referenceVelocityMps);
            std::ostringstream value; value.imbue(std::locale::classic()); value << std::setprecision(9) << k;
            asset.Set("damage", ""); asset.Set("damage_coefficient", value.str());
        }
        asset.Set("version", "3");
    }
    asset.Set("category", path);
}
Asset& FindIn(State& s, Kind kind, const std::string& id) {
    for (Asset& a : Collection(s, kind)) if (a.Id() == id) return a;
    throw std::runtime_error("Unknown asset: " + id);
}
std::string Folder(Kind kind) {
    return kind == Kind::Weapon ? "scripts/weapons/" : "scripts/cartridges/";
}
std::string Relative(const Asset& asset) {
    return Folder(asset.kind) + asset.Id() + (asset.kind == Kind::Weapon ? ".weapon" : ".cartridge");
}
// Check existing junctions/symlinks as well as lexical components. All generated
// paths remain inside this project; a linked scripts directory outside it fails.
fs::path SafePath(const fs::path& root, const std::string& rel) {
    const auto target = fs::weakly_canonical(root / fs::u8path(rel));
    auto r = root.begin(), t = target.begin();
    for (; r != root.end(); ++r, ++t)
        if (t == target.end() || *r != *t) throw std::runtime_error("Path leaves the game folder: " + rel);
    return root / fs::u8path(rel);
}
// Windows reserves device basenames even if an extension is appended (CON.weapon,
// LPT1.cartridge, etc.). Protect export on every host, not just Windows.
bool WindowsReservedName(const std::string& id) {
    const auto lower = Lower(id);
    for (const char* word : {"con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"})
        if (lower == word) return true;
    return false;
}
bool Read(const fs::path& path, std::string& data) {
    if (!fs::exists(path)) return false;
    if (!fs::is_regular_file(path)) throw std::runtime_error("Expected a regular file: " + path.u8string());
    if (fs::file_size(path) > kMaxFile) throw std::runtime_error("Definition exceeds 64 KiB: " + path.u8string());
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read: " + path.u8string());
    std::ostringstream text; text << in.rdbuf();
    if (in.bad()) throw std::runtime_error("Read failed: " + path.u8string());
    data = text.str();
    if (data.size() > kMaxFile) throw std::runtime_error("Definition exceeds 64 KiB: " + path.u8string());
    return true;
}
void Write(const fs::path& path, const std::string& data) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot write: " + path.u8string());
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.close();
    if (!out) throw std::runtime_error("Write failed: " + path.u8string());
}
std::vector<std::string> Manifest(const std::string& data) {
    if (data.size() > kMaxFile) throw std::runtime_error("Manifest exceeds 64 KiB.");
    if (data.find('\0') != std::string::npos) throw std::runtime_error("NUL in manifest.");
    std::vector<std::string> ids;
    std::set<std::string> seen;
    std::istringstream in(data); std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (!ValidId(line)) throw std::runtime_error("Invalid manifest ID: " + line);
        if (!seen.insert(Lower(line)).second) throw std::runtime_error("Duplicate manifest ID (Windows case-insensitive): " + line);
        ids.push_back(line);
        if (ids.size() > 256) throw std::runtime_error("Manifest exceeds 256 entries.");
    }
    return ids;
}
std::string Snapshot(const State& s) {
    std::string result;
    for (Kind kind : {Kind::Cartridge, Kind::Weapon}) {
        result += kind == Kind::Weapon ? "\n[weapons]\n" : "\n[cartridges]\n";
        for (const auto& a : Collection(s, kind)) {
            result += a.Id() + "\n";
            result += a.Serialize() + "\n";
        }
    }
    return result + "\n[editor categories]\n" + CategoryText(s.categories);
}
int NextRuntime(const State& s) {
    std::set<int> used;
    for (const auto& w : s.weapons) {
        // Include any numerically valid spelling, e.g. +32 and 032.
        try {
            size_t n = 0; int v = std::stoi(w.Get("runtime_id"), &n);
            if (n == w.Get("runtime_id").size()) used.insert(v);
        } catch (const std::exception&) {}
    }
    for (int i = modular::kFirstWeaponId; i <= modular::kLastWeaponId; ++i)
        if (!used.count(i)) return i;
    throw std::runtime_error("All modular runtime weapon IDs (32..62) are in use.");
}
bool Velocity(const std::string& value) {
    // Use the same classic-locale stream grammar as exported numeric values.
    std::istringstream in(value); in.imbue(std::locale::classic());
    double v = 0; char extra = 0;
    return (in >> v) && !(in >> extra) && std::isfinite(v) && v >= .01 && v <= 10000;
}
uint32_t U32(const unsigned char* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
float F32(const unsigned char* p) {
    const uint32_t bits = U32(p); float value = 0; std::memcpy(&value, &bits, sizeof(value)); return value;
}
std::string Label(const unsigned char* p, size_t n) {
    size_t len = 0; while (len < n && p[len]) ++len;
    return std::string(reinterpret_cast<const char*>(p), len);
}
} // namespace

bool ValidId(const std::string& id) {
    if (id.empty() || id.size() > 63) return false;
    for (char c : id)
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
            !(c >= '0' && c <= '9') && c != '_' && c != '-') return false;
    return true;
}
bool ValidCategory(const std::string& path, bool allowRoot) {
    if (path.empty()) return allowRoot;
    if (path.size() > 255) return false;
    size_t begin = 0, levels = 0;
    for (;;) {
        const auto end = path.find('/', begin);
        if (++levels > 16 || !ValidId(path.substr(begin, end == std::string::npos ? end : end-begin))) return false;
        if (end == std::string::npos) return true;
        begin = end+1;
    }
}
std::string CategoryParent(const std::string& path) {
    const auto slash = path.rfind('/');
    return slash == std::string::npos ? "" : path.substr(0, slash);
}
std::string Asset::Get(const std::string& key) const {
    for (const auto& f : fields) if (f.first == key) return f.second;
    return {};
}
void Asset::Set(const std::string& key, const std::string& value) {
    SingleLine(key); SingleLine(value);
    const auto found = std::find_if(fields.begin(), fields.end(), [&](const auto& f) { return f.first == key; });
    if (found != fields.end()) {
        if (value.empty()) fields.erase(found); else found->second = value;
    } else if (!value.empty()) fields.emplace_back(key, value);
}
std::string Asset::Serialize() const {
    std::string result;
    for (const auto& f : fields) if (!f.second.empty()) result += f.first + " = " + f.second + "\n";
    for (const auto& a : accepted) result += "accept = " + a.first + ":" + a.second + "\n";
    return result;
}
Asset Parse(const std::string& data, Kind kind) {
    if (data.size() > kMaxFile) throw std::runtime_error("Definition exceeds 64 KiB.");
    if (data.find('\0') != std::string::npos) throw std::runtime_error("NUL in definition.");
    Asset asset; asset.kind = kind;
    std::set<std::string> keys, cartridges;
    std::istringstream in(data); std::string line; int number = 0;
    while (std::getline(in, line)) {
        ++number; line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const auto equal = line.find('=');
        if (equal == std::string::npos) throw std::runtime_error("Line " + std::to_string(number) + ": expected key = value.");
        auto key = Trim(line.substr(0, equal)), value = Trim(line.substr(equal + 1));
        if (key.empty() || value.empty()) throw std::runtime_error("Line " + std::to_string(number) + ": empty key/value.");
        if (key == "accept" && kind == Kind::Weapon) {
            const auto colon = value.find(':');
            if (colon == std::string::npos) throw std::runtime_error("accept needs cartridge_id:velocity.");
            auto cid = Trim(value.substr(0, colon)), velocity = Trim(value.substr(colon + 1));
            if (!ValidId(cid) || velocity.empty() || !cartridges.insert(Lower(cid)).second)
                throw std::runtime_error("Invalid or duplicate accepted cartridge: " + cid);
            asset.accepted.emplace_back(cid, velocity);
        } else {
            if (!keys.insert(key).second) throw std::runtime_error("Duplicate field: " + key);
            asset.fields.emplace_back(key, value);
        }
    }
    return asset;
}
const std::vector<std::string>& FieldNames(Kind kind) {
    static const std::vector<std::string> cartridge = {"version", "id", "chamber", "projectile", "mass_grams",
        "diameter_mm", "drag_coefficient", "reference_velocity_mps", "projectile_count", "damage", "damage_coefficient", "category", "sprite"};
    static const std::vector<std::string> weapon = {"version", "id", "display_name", "runtime_id", "ammo_name",
        "default_cartridge", "clip_size", "default_ammo", "max_carry", "slot", "position", "weight", "flags",
        "npc_usable", "fire_interval", "spread", "reload_seconds", "range", "bullet_type", "muzzle_flash",
        "fire_underwater", "auto_reload", "idle_animation", "idle_seconds", "animation_extension",
        "primary_behavior", "primary_mode", "primary_interval", "primary_spread", "primary_projectiles",
        "primary_ammo_cost", "primary_burst_count", "primary_recoil_pitch", "primary_recoil_yaw",
        "primary_animation", "primary_sound", "primary_event", "secondary_behavior", "secondary_mode",
        "secondary_interval", "secondary_spread", "secondary_projectiles", "secondary_ammo_cost",
        "secondary_burst_count", "secondary_recoil_pitch", "secondary_recoil_yaw", "secondary_animation",
        "secondary_sound", "secondary_event", "view_model", "player_model", "world_model", "fire_sound",
        "reload_sound", "pickup_sound", "sprite", "fire_event", "fire_animation", "draw_animation", "reload_animation", "category"};
    return kind == Kind::Weapon ? weapon : cartridge;
}
fs::path DiscoverRoot(const fs::path& executable, const fs::path& explicitRoot) {
    if (!explicitRoot.empty()) return fs::weakly_canonical(fs::absolute(explicitRoot));
    fs::path dir = fs::weakly_canonical(fs::absolute(executable)).parent_path();
    const auto leaf = Lower(dir.filename().u8string());
    if (leaf == "devkit" || leaf == "bin" || leaf == "tools") dir = dir.parent_path();
    return dir;
}
const Asset* Project::Find(Kind kind, const std::string& id) const {
    for (const auto& a : Collection(state_, kind)) if (a.Id() == id) return &a;
    return nullptr;
}
std::vector<fs::path> Project::PendingRecovery(const fs::path& root) {
    std::vector<fs::path> found;
    if (!fs::is_directory(root)) return found;
    for (const auto& entry : fs::directory_iterator(root)) {
        if (!entry.is_directory() || entry.path().filename().u8string().find(".pxwe-stage-") != 0) continue;
        if (fs::exists(entry.path() / "committed.txt")) continue; // safe to discard, no rollback
        const auto backup = entry.path() / "backup";
        if (fs::is_regular_file(entry.path() / "targets.txt") ||
            (fs::is_directory(backup) &&
             fs::recursive_directory_iterator(backup) != fs::recursive_directory_iterator()))
            found.push_back(entry.path());
    }
    std::sort(found.begin(), found.end());
    return found;
}
void Project::RecoverInterruptedSave(const fs::path& root) {
    const auto game = fs::weakly_canonical(fs::absolute(root));
    const auto pending = PendingRecovery(game);
    if (pending.size() != 1) throw std::runtime_error("Recovery requires exactly one incomplete stage; inspect backups manually.");
    const fs::path stage = pending.front();
    std::string journal;
    if (!Read(stage / "targets.txt", journal))
        throw std::runtime_error("Stage journal is missing; preserve backups and recover manually: " + stage.u8string());
    std::set<std::string> targets;
    std::istringstream input(journal); std::string rel;
    while (std::getline(input, rel)) {
        if (rel.empty()) continue;
        // SafePath rejects escapes and enforces paths under the game folder.
        SafePath(game, rel);
        if (!targets.insert(rel).second) throw std::runtime_error("Duplicate recovery journal entry.");
    }
    if (targets.empty()) throw std::runtime_error("Empty recovery journal.");
    std::string originals;
    if (!Read(stage / "originals.txt", originals))
        throw std::runtime_error("Stage original-file journal missing; preserve backups for manual recovery.");
    std::set<std::string> preexisting;
    std::istringstream before(originals);
    while (std::getline(before, rel)) {
        if (rel.empty()) continue;
        if (!targets.count(rel)) throw std::runtime_error("Recovery original not in target journal.");
        preexisting.insert(rel);
    }
    // Remove any new installation for journaled files, then restore all known
    // backups. No user files outside the staged transaction are modified.
    for (const auto& name : targets) {
        const auto target = SafePath(game, name), backup = stage / "backup" / fs::u8path(name);
        if (!fs::is_regular_file(backup)) {
            if (!preexisting.count(name) && !fs::exists(stage / "new" / fs::u8path(name))) {
                // An originally absent file was installed; restore absence.
                std::error_code ec; fs::remove(target, ec);
                if (ec) throw std::runtime_error("Cannot remove newly-installed target: " + target.u8string());
            }
            // An original with no backup was never moved, or its backup was
            // already restored by a previous recovery attempt. Leave intact.
            continue;
        }
        std::error_code ec;
        fs::create_directories(target.parent_path());
        fs::remove(target, ec);
        if (ec) throw std::runtime_error("Unable to remove staged install: " + target.u8string());
        fs::rename(backup, target);
    }
    fs::remove_all(stage);
}
void Project::Load(const fs::path& root) {
    auto newRoot = fs::weakly_canonical(fs::absolute(root));
    if (!fs::is_directory(newRoot)) throw std::runtime_error("Choose an existing game/mod folder.");
    if (!PendingRecovery(newRoot).empty())
        throw std::runtime_error("Interrupted pxwe save detected. Restore staged backups first (File > Open offers recovery).");
    State loaded;
    std::map<std::string, File> baseline;
    for (Kind kind : {Kind::Cartridge, Kind::Weapon}) {
        const auto manifest = Folder(kind) + "manifest.txt";
        File m; m.exists = Read(SafePath(newRoot, manifest), m.contents);
        baseline[manifest] = m;
        for (const auto& id : Manifest(m.contents)) {
            const std::string rel = Folder(kind) + id + (kind == Kind::Weapon ? ".weapon" : ".cartridge");
            File file; file.exists = Read(SafePath(newRoot, rel), file.contents);
            if (!file.exists) throw std::runtime_error("Manifest asset is missing: " + rel);
            auto asset = Parse(file.contents, kind);
            if (asset.Id() != id) throw std::runtime_error("Manifest/definition ID mismatch: " + rel);
            Collection(loaded, kind).push_back(std::move(asset));
            baseline[rel] = std::move(file);
        }
    }
    File folders; folders.exists = Read(SafePath(newRoot, kCategoryFile), folders.contents);
    if (folders.exists) loaded.categories = ParseCategories(folders.contents);
    baseline[kCategoryFile] = std::move(folders);
    root_ = std::move(newRoot); state_ = std::move(loaded); baseline_ = std::move(baseline);
    saved_ = Snapshot(state_); undo_.clear(); redo_.clear();
}
void Project::Commit(State state) {
    if (Snapshot(state) == Snapshot(state_)) return;
    undo_.push_back(state_);
    if (undo_.size() > 100) undo_.erase(undo_.begin());
    redo_.clear(); state_ = std::move(state);
}
void Project::CheckNewId(Kind kind, const std::string& id) const {
    if (root_.empty()) throw std::runtime_error("Open a game folder first.");
    if (!ValidId(id) || WindowsReservedName(id)) throw std::runtime_error("IDs use ASCII letters, digits, _ or - (1..63 characters) and cannot be reserved Windows names such as CON, NUL or COM1.");
    for (const auto& a : Collection(state_, kind))
        if (Lower(a.Id()) == Lower(id)) throw std::runtime_error("Asset ID already exists (Windows case-insensitive).");
    // An unlisted definition belongs to its existing author. Reject collisions
    // at creation as well as export, including Windows spellings on Linux.
    const auto folder = SafePath(root_, Folder(kind));
    const auto leaf = Lower(id + (kind == Kind::Weapon ? ".weapon" : ".cartridge"));
    if (fs::exists(folder)) for (const auto& entry : fs::directory_iterator(folder)) {
        if (Lower(entry.path().filename().u8string()) != leaf) continue;
        const auto rel = Folder(kind) + entry.path().filename().u8string();
        // Delete then re-create an owned definition is a supported operation.
        const auto owned = baseline_.find(rel);
        if (owned == baseline_.end() || !owned->second.exists)
            throw std::runtime_error("A definition with this ID already exists on disk: " + rel);
        // Reusing a differently cased filename would leave a duplicate on
        // case-sensitive platforms and cannot round-trip through Windows.
        if (entry.path().filename().u8string() != id + (kind == Kind::Weapon ? ".weapon" : ".cartridge"))
            throw std::runtime_error("Use the existing definition's exact ID spelling: " + rel);
    }
}
void Project::Add(Kind kind, const std::string& id, const std::string& copyId, const std::optional<std::string>& category, bool legacyTemplate) {
    CheckNewId(kind, id);
    if (category && !ValidCategory(*category, true)) throw std::runtime_error("Invalid category path.");
    State next = state_;
    Asset asset; asset.kind = kind;
    if (!copyId.empty()) asset = FindIn(next, kind, copyId);
    else if (kind == Kind::Cartridge) {
        modular::Cartridge c; c.id = id; c.chamber = "9x19"; c.projectileType = "fmj";
        c.schemaVersion = legacyTemplate ? 2 : 4; c.attackLogic = modular::AttackLogic::Hitscan;
        c.damageLogic = modular::DamageLogic::Legacy; c.defaultDamage = 20.f;
        c.legacyBulletType = 1; c.massGrams = 8.04f; c.diameterMm = 9.02f;
        c.dragCoefficient = .3f; c.referenceVelocityMps = 360.f;
        asset = Parse(modular::SerializeCartridge(c), kind);
    } else {
        modular::Weapon w; w.id = id; w.schemaVersion = legacyTemplate ? 2 : 4; w.displayName = id; w.clipSize = 15; w.defaultAmmo = 30;
        w.maxCarry = 120; w.slot = 1; w.npcUsable = true;
        w.viewModel = "models/v_" + id + ".mdl"; w.playerModel = "models/p_" + id + ".mdl";
        w.worldModel = "models/w_" + id + ".mdl"; w.fireAnimation = 3; w.drawAnimation = 5; w.reloadAnimation = 6;
        w.primary.behavior = legacyTemplate ? "ballistic" : "fire_loaded_cartridge"; w.primary.mode = "semi"; w.primary.animation = 3; w.primary.recoilPitch = -2;
        w.secondary.behavior = "ammo_cycle"; w.secondary.mode = "semi"; w.secondary.ammoCost = 0;
        w.ammoName = w.defaultCartridgeId = "new_cartridge"; // editable draft if no cartridges exist yet
        const Asset* firstCompatible = nullptr;
        for (const auto& item : next.cartridges) {
            if ((item.Get("version") == "4") == !legacyTemplate) { firstCompatible = &item; break; }
        }
        if (firstCompatible) {
            w.defaultCartridgeId = w.ammoName = firstCompatible->Id();
            w.compatible.push_back({w.defaultCartridgeId, 360});
        }
        asset = Parse(modular::SerializeWeapon(w), kind);
        if (firstCompatible)
            asset.accepted.front().second = firstCompatible->Get("reference_velocity_mps");
    }
    asset.Set("id", id);
    if (kind == Kind::Weapon) asset.Set("runtime_id", std::to_string(NextRuntime(next)));
    if (category) AssignCategory(asset, *category);
    Collection(next, kind).push_back(std::move(asset)); Commit(std::move(next));
}
void Project::Remove(Kind kind, const std::string& id) {
    if (!Find(kind, id)) throw std::runtime_error("Unknown asset: " + id);
    if (kind == Kind::Cartridge) for (const auto& w : state_.weapons) {
        if (w.Get("default_cartridge") == id || w.Get("ammo_name") == id ||
            std::any_of(w.accepted.begin(), w.accepted.end(), [&](const auto& a) { return a.first == id; }))
            throw std::runtime_error("Cartridge is used by weapon " + w.Id() + ".");
    }
    State next = state_; auto& items = Collection(next, kind);
    items.erase(std::remove_if(items.begin(), items.end(), [&](const Asset& a) { return a.Id() == id; }), items.end());
    Commit(std::move(next));
}
void Project::RenameId(Kind kind, const std::string& oldId, const std::string& newId) {
    if (root_.empty()) throw std::runtime_error("Open a game folder before renaming.");
    if (!Find(kind, oldId)) throw std::runtime_error("Select an existing asset to rename.");
    if (!ValidId(newId) || WindowsReservedName(newId)) throw std::runtime_error("New ID must contain 1..63 ASCII letters, digits, _ or - and must not be a Windows device name such as CON, NUL or COM1.");
    if (newId == oldId) return;
    for (const auto& asset : Collection(state_, kind)) {
        if (asset.Id() != oldId && Lower(asset.Id()) == Lower(newId))
            throw std::runtime_error("Another asset already uses this ID (Windows ignores case).");
    }
    // On disk, only the old listed definition may collide with the new ID.
    // Never overwrite an unlisted file or a differently named listed asset.
    const auto folder = SafePath(root_, Folder(kind));
    const std::string extension = kind == Kind::Weapon ? ".weapon" : ".cartridge";
    if (fs::exists(folder)) for (const auto& entry : fs::directory_iterator(folder)) {
        if (Lower(entry.path().filename().u8string()) != Lower(newId + extension)) continue;
        if (entry.path().filename().u8string() == oldId + extension) continue;
        throw std::runtime_error("Destination filename already exists (including Windows case aliases): " + entry.path().filename().u8string());
    }
    State next = state_;
    FindIn(next, kind, oldId).Set("id", newId);
    if (kind == Kind::Cartridge) {
        const auto fromPrefix = "ammo." + oldId + ".";
        const auto toPrefix = "ammo." + newId + ".";
        for (auto& weapon : next.weapons) {
            for (auto& accepted : weapon.accepted)
                if (accepted.first == oldId) accepted.first = newId;
            if (weapon.Get("default_cartridge") == oldId) weapon.Set("default_cartridge", newId);
            if (weapon.Get("ammo_name") == oldId) weapon.Set("ammo_name", newId);
            for (auto& field : weapon.fields) {
                if (field.first.compare(0, fromPrefix.size(), fromPrefix) == 0)
                    field.first = toPrefix + field.first.substr(fromPrefix.size());
                // Only a typed conditional asset's `when` field is an ammo selector;
                // do not touch arbitrary strings which happen to contain the old ID.
                if (field.first.compare(0, 6, "asset.") == 0 &&
                    field.first.size() > 11 && field.first.substr(field.first.size() - 5) == ".when" &&
                    field.second == "ammo:" + oldId)
                    field.second = "ammo:" + newId;
            }
        }
    }
    // Registry validation covers manifest, accepted ammo, defaults, overrides,
    // conditional assets and value constraints before any undo history changes.
    CheckedCommit(std::move(next));
}
void Project::SetField(Kind kind, const std::string& id, const std::string& key, const std::string& value) {
    if (key == "id") throw std::runtime_error("Asset IDs are read-only. Use Duplicate to create a new ID.");
    if (key == "category") { SetCategory(kind, id, value); return; }
    SingleLine(key); SingleLine(value);
    State next = state_; auto& item = FindIn(next, kind, id);
    const auto trimmed = Trim(value);
    // Switching v4 physics/damage in the editor is one undoable transaction.
    // Never leave mutually exclusive damage values behind in the authored file.
    if (kind == Kind::Cartridge && item.Get("version") == "4" &&
        (key == "attack_logic" || key == "damage_logic")) {
        auto logic = key == "attack_logic" ? trimmed : item.Get("attack_logic");
        auto damage = key == "damage_logic" ? trimmed : item.Get("damage_logic");
        if (logic != "hitscan" && logic != "ballistic") throw std::runtime_error("Choose Hitscan or Ballistic Attack Logic.");
        if (key == "attack_logic") damage = logic == "hitscan" ? "legacy" :
            item.Get("damage_logic") == "energy_based" ? "energy_based" : "configurable_base";
        if ((logic == "hitscan" && damage != "legacy") ||
            (logic == "ballistic" && damage != "configurable_base" && damage != "energy_based"))
            throw std::runtime_error("Damage Logic is not available for this Attack Logic. Change Attack Logic first.");
        item.Set("attack_logic", logic); item.Set("damage_logic", damage);
        if (damage == "legacy") {
            item.Set("base_damage", ""); item.Set("damage_coefficient", "");
            if (item.Get("default_damage").empty()) item.Set("default_damage", "20");
            if (item.Get("legacy_bullet_type").empty()) item.Set("legacy_bullet_type", "1");
        } else {
            item.Set("default_damage", ""); item.Set("legacy_bullet_type", "");
            if (damage == "configurable_base") {
                item.Set("damage_coefficient", "");
                if (item.Get("base_damage").empty()) item.Set("base_damage", "20");
            } else {
                item.Set("base_damage", "");
                if (item.Get("damage_coefficient").empty()) item.Set("damage_coefficient", "1");
            }
        }
    } else item.Set(key, trimmed);
    Commit(std::move(next));
}
void Project::Allow(const std::string& weaponId, const std::string& cartridgeId, const std::string& velocity) {
    if (!Find(Kind::Cartridge, cartridgeId)) throw std::runtime_error("Unknown cartridge.");
    if (!Velocity(velocity)) throw std::runtime_error("Muzzle velocity must be 0.01..10000 metres/second.");
    State next = state_; auto& accepted = FindIn(next, Kind::Weapon, weaponId).accepted;
    const auto it = std::find_if(accepted.begin(), accepted.end(), [&](const auto& a) { return a.first == cartridgeId; });
    if (it == accepted.end()) accepted.emplace_back(cartridgeId, Trim(velocity)); else it->second = Trim(velocity);
    Commit(std::move(next));
}
void Project::Disallow(const std::string& weaponId, const std::string& cartridgeId) {
    State next = state_; auto& w = FindIn(next, Kind::Weapon, weaponId);
    if (w.Get("default_cartridge") == cartridgeId) throw std::runtime_error("Choose another default cartridge first.");
    for (const auto& f : w.fields) if (f.first.compare(0, 5 + cartridgeId.size() + 1, "ammo." + cartridgeId + ".") == 0 ||
        (f.first.compare(0, 6, "asset.") == 0 && f.second == "ammo:" + cartridgeId))
        throw std::runtime_error("Remove this cartridge's overrides and conditional entries before disallowing it.");
    auto& accepted = w.accepted;
    accepted.erase(std::remove_if(accepted.begin(), accepted.end(), [&](const auto& a) { return a.first == cartridgeId; }), accepted.end());
    Commit(std::move(next));
}
void Project::SetDefault(const std::string& weaponId, const std::string& cartridgeId) {
    State next = state_; auto& w = FindIn(next, Kind::Weapon, weaponId);
    if (std::none_of(w.accepted.begin(), w.accepted.end(), [&](const auto& a) { return a.first == cartridgeId; }))
        throw std::runtime_error("Allow this cartridge before choosing it as default.");
    w.Set("default_cartridge", cartridgeId); w.Set("ammo_name", cartridgeId); Commit(std::move(next));
}
void Project::CheckedCommit(State state) {
    Project staged = *this; staged.state_ = state;
    const auto result = staged.Validate();
    if (!result.Ok()) throw std::runtime_error(result.Message());
    Commit(std::move(state));
}
std::string Project::SuggestedCoefficient(const std::string& cartridgeId) const {
    const auto result = Validate();
    if (!result.Ok()) throw std::runtime_error(result.Message());
    const auto* c = result.registry.FindCartridge(cartridgeId);
    if (!c) throw std::runtime_error("Unknown cartridge.");
    const double coefficient = c->schemaVersion >= 3 ? c->damageCoefficient :
        c->damage * modular::ProjectileAreaMm2(c->diameterMm) /
        modular::KineticEnergyJ(c->massGrams, c->referenceVelocityMps);
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::setprecision(9) << coefficient;
    return out.str();
}
void Project::SetCoefficient(const std::string& cartridgeId, const std::string& value) {
    SingleLine(value);
    State next = state_; auto& c = FindIn(next, Kind::Cartridge, cartridgeId);
    if (c.Get("version") == "4") {
        if (c.Get("damage_logic") != "energy_based")
            throw std::runtime_error("Damage Coefficient is only available for Energy-Based Damage Logic.");
    } else { c.Set("version", "3"); c.Set("damage", ""); }
    c.Set("damage_coefficient", Trim(value));
    CheckedCommit(std::move(next));
}
void Project::PutVariant(const std::string& weaponId, const std::string& id,
    const std::string& role, const std::string& path, const std::string& when, const std::string& replaceId) {
    if (!ValidId(id)) throw std::runtime_error("Variant ID uses 1..63 ASCII letters, digits, _ or -.");
    SingleLine(role); SingleLine(path); SingleLine(when);
    State next = state_; auto& w = FindIn(next, Kind::Weapon, weaponId);
    const auto prefix = "asset." + id + ".";
    for (const auto& f : w.fields) if (f.first.compare(0, 6, "asset.") == 0) {
        const auto end = f.first.find('.', 6); const auto existing = f.first.substr(6, end - 6);
        if (Lower(existing) == Lower(id) && existing != replaceId)
            throw std::runtime_error("Variant ID already exists.");
    }
    if (!replaceId.empty()) {
        const auto old = "asset." + replaceId + ".";
        if (w.Get(old + "role").empty()) throw std::runtime_error("Unknown conditional entry.");
        w.fields.erase(std::remove_if(w.fields.begin(), w.fields.end(), [&](const auto& f) {
            return f.first.compare(0, old.size(), old) == 0;
        }), w.fields.end());
    }
    if (w.Get("version") != "4") w.Set("version", "3");
    w.Set(prefix + "role", Trim(role));
    w.Set(prefix + "path", Trim(path)); w.Set(prefix + "when", Trim(when));
    CheckedCommit(std::move(next));
}
void Project::RemoveVariant(const std::string& weaponId, const std::string& id) {
    State next = state_; auto& w = FindIn(next, Kind::Weapon, weaponId);
    const auto prefix = "asset." + id + ".";
    if (w.Get(prefix + "role").empty()) throw std::runtime_error("Unknown conditional entry.");
    w.fields.erase(std::remove_if(w.fields.begin(), w.fields.end(), [&](const auto& f) {
        return f.first.compare(0, prefix.size(), prefix) == 0;
    }), w.fields.end());
    CheckedCommit(std::move(next));
}
void Project::SetAmmoOverrides(const std::string& weaponId, const std::string& cartridgeId, const Fields& values) {
    State next = state_; auto& w = FindIn(next, Kind::Weapon, weaponId);
    if (std::none_of(w.accepted.begin(), w.accepted.end(), [&](const auto& a) { return a.first == cartridgeId; }))
        throw std::runtime_error("Allow this cartridge before setting overrides.");
    std::set<std::string> roles = {"view_model", "player_model", "world_model", "fire_sound", "reload_sound", "sprite", "hud_sprite", "kill_sprite"};
    const bool v4 = w.Get("version") == "4";
    if (v4) { roles.insert("fire_rate"); roles.insert("spread"); }
    const auto prefix = "ammo." + cartridgeId + ".";
    std::set<std::string> supplied;
    bool changed = false;
    for (const auto& f : values) {
        if (!roles.count(f.first) || !supplied.insert(f.first).second) throw std::runtime_error("Unknown or repeated override role.");
        SingleLine(f.second); const auto value = Trim(f.second);
        if (!value.empty() && (f.first == "fire_rate" || f.first == "spread")) {
            // Reject locale-specific decimals, non-finite numbers, negative spread,
            // and out-of-range RPM before entering the undo stack.
            size_t consumed=0; double number=0;
            try { number=std::stod(value, &consumed); } catch (...) { throw std::runtime_error("Fire Rate / Spread must be a valid number."); }
            if (consumed != value.size() || !std::isfinite(number) ||
                (f.first == "fire_rate" ? number<1 || number>6000 : number<0 || number>1))
                throw std::runtime_error(f.first == "fire_rate" ? "Fire Rate must be 1..6000 RPM; blank inherits." : "Spread must be 0..1; blank inherits.");
        }
        changed |= w.Get(prefix + f.first) != value; w.Set(prefix + f.first, value);
    }
    if (!changed) return;
    if (!v4) w.Set("version", "3");
    CheckedCommit(std::move(next));
}
void Project::SetSprite(Kind kind, const std::string& id, const std::string& path) {
    SingleLine(path);
    if (kind == Kind::Cartridge && !Trim(path).empty() && Find(kind, id) &&
        Find(kind, id)->Get("version") != "3" && Find(kind, id)->Get("version") != "4") {
        // An explicit ammo-icon edit also converts legacy damage, preserving
        // damage at the reference speed. Neither half of the conversion leaks.
        const auto k = SuggestedCoefficient(id);
        State next = state_; auto& c = FindIn(next, kind, id);
        c.Set("version", "3"); c.Set("damage", ""); c.Set("damage_coefficient", k); c.Set("sprite", Trim(path));
        CheckedCommit(std::move(next)); return;
    }
    State next = state_; FindIn(next, kind, id).Set("sprite", Trim(path)); CheckedCommit(std::move(next));
}
std::vector<std::string> Project::Categories(Kind kind) const { return CategoryPaths(state_, kind); }
void Project::CreateCategory(Kind kind, const std::string& path) {
    if (root_.empty()) throw std::runtime_error("Open a game folder first.");
    if (!ValidCategory(path)) throw std::runtime_error("Use a slash-separated category path of ASCII IDs (up to 16 levels / 255 characters).");
    const auto paths = Categories(kind);
    if (std::find(paths.begin(), paths.end(), path) != paths.end()) throw std::runtime_error("Category already exists.");
    State next = state_; next.categories.push_back({kind, path});
    CategoryText(next.categories); Commit(std::move(next));
}
void Project::SetCategory(Kind kind, const std::string& id, const std::string& path) {
    SingleLine(path); const auto target = Trim(path);
    State next = state_; AssignCategory(FindIn(next, kind, id), target); CheckedCommit(std::move(next));
}
void Project::RelocateCategory(Kind kind, const std::string& from, const std::string& to) {
    const auto paths = Categories(kind);
    if (!ValidCategory(from) || std::find(paths.begin(), paths.end(), from) == paths.end()) throw std::runtime_error("Select an existing category.");
    if (!ValidCategory(to)) throw std::runtime_error("Invalid destination category path.");
    if (from == to) return;
    if (InCategory(to, from)) throw std::runtime_error("A category cannot be moved inside itself.");
    if (std::find(paths.begin(), paths.end(), to) != paths.end()) throw std::runtime_error("Destination category already exists; choose a new name.");
    State next = state_;
    for (auto& a : Collection(next, kind)) if (InCategory(a.Get("category"), from))
        AssignCategory(a, to+a.Get("category").substr(from.size()));
    for (auto& c : next.categories) if (c.kind == kind && InCategory(c.path, from)) c.path = to+c.path.substr(from.size());
    // Keep the relocated folder even when all its assets are later moved away.
    if (std::none_of(next.categories.begin(), next.categories.end(), [&](const auto& c){return c.kind==kind && c.path==to;}))
        next.categories.push_back({kind,to});
    CheckedCommit(std::move(next));
}
void Project::DuplicateCategory(Kind kind, const std::string& from, const std::string& to, const std::string& idPrefix) {
    const auto paths = Categories(kind);
    if (!ValidCategory(from) || std::find(paths.begin(), paths.end(), from) == paths.end()) throw std::runtime_error("Select an existing category.");
    if (!ValidCategory(to) || InCategory(to, from)) throw std::runtime_error("Choose a new category outside the source subtree.");
    if (!ValidId(idPrefix)) throw std::runtime_error("Duplicate IDs need an ASCII ID prefix.");
    if (std::find(paths.begin(), paths.end(), to) != paths.end()) throw std::runtime_error("Destination category already exists.");
    State next = state_;
    for (const auto& original : Collection(state_, kind)) if (InCategory(original.Get("category"), from)) {
        const auto id = idPrefix+"_"+original.Id(); CheckNewId(kind,id);
        Asset copy = original; copy.Set("id",id);
        AssignCategory(copy, to+original.Get("category").substr(from.size()));
        if (kind == Kind::Weapon) copy.Set("runtime_id",std::to_string(NextRuntime(next)));
        Collection(next,kind).push_back(std::move(copy));
    }
    for (const auto& c : state_.categories) if (c.kind == kind && InCategory(c.path,from)) next.categories.push_back({kind,to+c.path.substr(from.size())});
    if (std::none_of(next.categories.begin(),next.categories.end(),[&](const auto& c){return c.kind==kind && c.path==to;})) next.categories.push_back({kind,to});
    CheckedCommit(std::move(next));
}
void Project::RemoveCategory(Kind kind, const std::string& path, bool deleteAssets) {
    const auto paths = Categories(kind);
    if (!ValidCategory(path) || std::find(paths.begin(),paths.end(),path)==paths.end()) throw std::runtime_error("Select an existing category.");
    State next = state_; auto& items = Collection(next,kind);
    if (deleteAssets) items.erase(std::remove_if(items.begin(),items.end(),[&](const auto& a){return InCategory(a.Get("category"),path);}),items.end());
    else for (auto& a : items) if (InCategory(a.Get("category"),path)) AssignCategory(a,CategoryParent(path));
    next.categories.erase(std::remove_if(next.categories.begin(),next.categories.end(),[&](const auto& c){return c.kind==kind && InCategory(c.path,path);}),next.categories.end());
    CheckedCommit(std::move(next));
}
bool Project::Undo() {
    if (undo_.empty()) return false;
    redo_.push_back(state_); state_ = std::move(undo_.back()); undo_.pop_back(); return true;
}
bool Project::Redo() {
    if (redo_.empty()) return false;
    undo_.push_back(state_); state_ = std::move(redo_.back()); redo_.pop_back(); return true;
}
bool Project::Dirty() const { return Snapshot(state_) != saved_; }
Project::Files Project::ExportFiles() const {
    Files files;
    const auto folders = CategoryText(state_.categories);
    if (!folders.empty()) files[kCategoryFile] = folders;
    for (Kind kind : {Kind::Cartridge, Kind::Weapon}) {
        std::string manifest;
        std::set<std::string> ids;
        for (const auto& a : Collection(state_, kind)) {
            if (!ValidId(a.Id()) || !ids.insert(Lower(a.Id())).second)
                throw std::runtime_error("Invalid or duplicate asset ID.");
            manifest += a.Id() + "\n"; files[Relative(a)] = a.Serialize();
        }
        files[Folder(kind) + "manifest.txt"] = manifest;
    }
    return files;
}
std::string Validation::Message() const {
    std::string text;
    for (const auto& error : errors) text += error + "\n";
    if (Ok()) {
        text = "Script-valid: " + std::to_string(registry.Weapons().size()) + " weapons, " +
            std::to_string(registry.Cartridges().size()) + " cartridges. Fingerprint: " +
            std::to_string(registry.Fingerprint()) + "\n";
        text += admissionKnown ? (admissionPassed ? "Engine admission: passed using supplied engine registration state.\n" :
            "Engine admission: FAILED.\n") :
            "Engine admission: UNKNOWN (no real legacy ammo/weapon registration data supplied).\n";
    }
    for (const auto& warning : warnings) text += "Warning: " + warning + "\n";
    return text;
}
Validation Project::Validate(const std::optional<RuntimeAdmission>& engine) const {
    Validation validation;
    try {
        const auto files = ExportFiles();
        validation.registry.Load([&](const std::string& rel, std::string& data) {
            const auto it = files.find(rel); if (it == files.end()) return false;
            data = it->second; return true;
        });
        for (const auto& d : validation.registry.Diagnostics())
            validation.errors.push_back(d.file + ":" + std::to_string(d.line) + ": " + d.message);
        if (!validation.registry.Valid() && validation.errors.empty()) validation.errors.push_back("Registry rejected the definitions.");
        std::string error;
        // Best-case capacity test can reject impossible packs, but it is not
        // proof of admission: original game weapon/ammo registration occurs later.
        if (validation.registry.Valid() && !validation.registry.ValidateForRuntime(32, 0, {}, {}, error))
            validation.errors.push_back("Capacity upper bound: " + error);
        if (engine && validation.registry.Valid()) {
            validation.admissionKnown = true;
            validation.admissionPassed = validation.registry.ValidateForRuntime(
                engine->ammoSlots, engine->usedAmmoSlots, engine->occupiedAmmoNames,
                engine->occupiedWeaponIds, error);
            if (!validation.admissionPassed)
                validation.errors.push_back("Engine admission preflight: " + error);
        }
        // Loose-file existence is only an advisory test. Xash may supply a
        // resource from pak/VFS mounts that a standalone editor cannot see.
        if (!root_.empty()) {
            std::set<std::string> paths;
            const auto collect = [&](const std::string& key, const std::string& value) {
                if (value.empty()) return;
                const bool sound = key.find("sound") != std::string::npos;
                const bool model = key.find("model") != std::string::npos;
                const bool sprite = key.find("sprite") != std::string::npos;
                const bool event = key.find("event") != std::string::npos;
                if (!sound && !model && !sprite && !event && key.find(".path") == std::string::npos) return;
                if (value.find("..") != std::string::npos || value[0] == '/' || value[0] == '\\') return;
                std::string rel = sound ? "sound/" + value : value;
                if (key.find(".path") != std::string::npos &&
                    rel.find("models/") != 0 && rel.find("sprites/") != 0 && rel.find("events/") != 0 &&
                    rel.find("sound/") != 0 && rel.size() >= 4 && rel.substr(rel.size()-4)==".wav")
                    rel = "sound/" + rel;
                paths.insert(rel);
            };
            for (const auto& weapon : state_.weapons) for (const auto& entry : weapon.fields)
                collect(entry.first, entry.second);
            for (const auto& cartridge : state_.cartridges) for (const auto& entry : cartridge.fields)
                collect(entry.first, entry.second);
            for (const auto& path : paths) {
                std::error_code ec;
                if (fs::is_regular_file(root_ / fs::u8path(path), ec)) continue;
                // Reuse the previewer's bounded pak*.pak reader. Mounts from
                // other game folders or custom VFS modules are still unknown.
                try { (void)ReadResource(root_, path); }
                catch (const std::exception&) {
                    validation.warnings.push_back("resource not found in loose files or local PAK; "
                        "may be in base-game/VFS mounts: " + path);
                }
            }
        }
    } catch (const std::exception& e) { validation.errors.push_back(e.what()); }
    return validation;
}
void Project::Save() {
    const auto validation = Validate();
    if (!validation.Ok()) throw std::runtime_error(validation.Message());
    if (root_.empty()) throw std::runtime_error("Open a game folder first.");
    if (!PendingRecovery(root_).empty())
        throw std::runtime_error("Unfinished pxwe save detected; recover it before saving.");
    const auto files = ExportFiles();
    // Reject changes made by another editor since Load/Save, and never overwrite
    // a pre-existing asset that wasn't owned by the loaded manifests.
    std::set<std::string> targets;
    for (const auto& f : baseline_) targets.insert(f.first);
    for (const auto& f : files) targets.insert(f.first);
    for (const auto& rel : targets) {
        File now; now.exists = Read(SafePath(root_, rel), now.contents);
        const auto old = baseline_.find(rel);
        if (old == baseline_.end()) {
            if (now.exists) throw std::runtime_error("Unlisted file already exists; choose a different asset ID: " + rel);
        } else if (now.exists != old->second.exists || now.contents != old->second.contents)
            throw std::runtime_error("File changed outside pxwe. Reopen the project before saving: " + rel);
    }
    fs::path stage;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto candidate = root_ / (".pxwe-stage-" + std::to_string(stamp) + "-" + std::to_string(attempt));
        if (fs::create_directory(candidate)) { stage = candidate; break; }
    }
    if (stage.empty()) throw std::runtime_error("Cannot create a staging directory in the game folder.");
    std::vector<std::string> backedUp, installed;
    try {
        for (const auto& f : files) Write(stage / "new" / fs::u8path(f.first), f.second);
        // The journal and staged files are written before touching live files.
        std::string journal, originals;
        for (const auto& rel : targets) {
            journal += rel + "\n";
            if (fs::exists(SafePath(root_, rel))) originals += rel + "\n";
        }
        Write(stage / "originals.txt", originals);
        Write(stage / "targets.txt", journal);
        // Backups also own deletions; rollback can restore removed definitions.
        for (const auto& rel : targets) {
            const auto target = SafePath(root_, rel);
            if (!fs::exists(target)) continue;
            auto backup = stage / "backup" / fs::u8path(rel);
            fs::create_directories(backup.parent_path());
            fs::rename(target, backup); backedUp.push_back(rel);
        }
        // Data first, manifests last. Only validated, staged bytes are installed.
        for (int manifestPass = 0; manifestPass < 2; ++manifestPass) for (const auto& f : files) {
            const bool manifest = fs::path(f.first).filename() == "manifest.txt";
            if (manifest != bool(manifestPass)) continue;
            const auto target = SafePath(root_, f.first);
            fs::create_directories(target.parent_path());
            fs::rename(stage / "new" / fs::u8path(f.first), target); installed.push_back(f.first);
        }
        // Only mark the transaction committed after all live files are placed.
        Write(stage / "committed.txt", "installation complete\n");
    } catch (const std::exception& e) {
        const std::string reason = e.what(); bool restored = true;
        for (auto it = installed.rbegin(); it != installed.rend(); ++it) {
            std::error_code ec; fs::remove(root_ / fs::u8path(*it), ec); if (ec) restored = false;
        }
        for (auto it = backedUp.rbegin(); it != backedUp.rend(); ++it) {
            std::error_code ec; fs::rename(stage / "backup" / fs::u8path(*it), root_ / fs::u8path(*it), ec);
            if (ec) restored = false;
        }
        if (restored) { std::error_code ec; fs::remove_all(stage, ec); }
        throw std::runtime_error(reason + (restored ? "\nOriginal files restored." :
            "\nRecovery files retained in: " + stage.u8string()));
    }
    // Update the session before best-effort cleanup; cleanup failure must not
    // report a successful export as an unsaved project or attempt another write.
    baseline_.clear();
    for (const auto& f : files) baseline_[f.first] = {true, f.second};
    saved_ = Snapshot(state_);
    std::error_code ec; fs::remove_all(stage, ec);
}
ModelInfo ReadModel(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open model: " + path.u8string());
    const auto actual = fs::file_size(path);
    unsigned char header[244]{};
    if (!in.read(reinterpret_cast<char*>(header), sizeof(header)) ||
        std::memcmp(header, "IDST", 4) != 0 || U32(header + 4) != 10)
        throw std::runtime_error("Expected a GoldSrc Studio v10 model.");
    const uint32_t length = U32(header + 72), count = U32(header + 164), offset = U32(header + 168);
    if (length < sizeof(header) || length > actual || count > 2048 ||
        (count && (offset < sizeof(header) || uint64_t(offset) + uint64_t(count) * 176 > length)))
        throw std::runtime_error("Invalid Studio model length or sequence table.");
    ModelInfo info; info.name = Label(header + 8, 64);
    for (int i = 0; i < 3; ++i) {
        info.mins[i] = F32(header + 88 + i * 4); info.maxs[i] = F32(header + 100 + i * 4);
        if (!std::isfinite(info.mins[i]) || !std::isfinite(info.maxs[i]) || info.mins[i] > info.maxs[i])
            throw std::runtime_error("Invalid Studio model bounds.");
    }
    in.seekg(offset);
    for (uint32_t i = 0; i < count; ++i) {
        unsigned char seq[176]{};
        if (!in.read(reinterpret_cast<char*>(seq), sizeof(seq))) throw std::runtime_error("Truncated Studio sequence.");
        const float fps = F32(seq + 32); const uint32_t frames = U32(seq + 56);
        if (!std::isfinite(fps) || fps < 0 || frames > uint32_t(std::numeric_limits<int>::max()))
            throw std::runtime_error("Invalid Studio sequence metadata.");
        info.sequences.push_back({Label(seq, 32), fps, static_cast<int>(frames)});
    }
    return info;
}
std::vector<modular::BallisticState> Flight(const modular::Cartridge& cartridge, double velocity) {
    if (!std::isfinite(velocity) || velocity < .01 || velocity > 10000 ||
        !std::isfinite(cartridge.massGrams) || cartridge.massGrams <= 0 ||
        !std::isfinite(cartridge.diameterMm) || cartridge.diameterMm <= 0 ||
        !std::isfinite(cartridge.dragCoefficient) || cartridge.dragCoefficient < 0)
        throw std::runtime_error("Invalid ballistic preview parameters.");
    modular::BallisticParameters params;
    params.massKg = cartridge.massGrams / 1000.0; params.diameterM = cartridge.diameterMm / 1000.0;
    params.dragCoefficient = cartridge.dragCoefficient;
    modular::BallisticState state; state.velocity.x = velocity;
    std::vector<modular::BallisticState> states{state};
    for (int i = 0; i < 400; ++i) {
        state = modular::IntegrateBallistic(state, params, .005);
        if ((i + 1) % 10 == 0) states.push_back(state);
    }
    return states;
}
} // namespace pxwe
