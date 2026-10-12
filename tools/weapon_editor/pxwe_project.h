// Native weapon editor backend. No engine, Python, or window-system dependencies.
// Copyright (C) 2026 PrimeXT contributors. GPL-3.0-or-later.
#pragma once
#include "modular_weapons.h"
#include "ballistic_flight.h"
#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace pxwe {
namespace fs = std::filesystem;
enum class Kind { Cartridge, Weapon };
using Fields = std::vector<std::pair<std::string, std::string>>;
struct Asset {
    Kind kind = Kind::Cartridge;
    Fields fields, accepted;
    std::string Get(const std::string& key) const;
    std::string Id() const { return Get("id"); }
    void Set(const std::string& key, const std::string& value);
    std::string Serialize() const;
};
struct Category { Kind kind = Kind::Weapon; std::string path; };
struct State {
    std::vector<Asset> cartridges, weapons;
    // Explicit folders preserve empty categories. Asset categories also imply
    // ancestors, so existing format-3 packs need no editor sidecar.
    std::vector<Category> categories;
};
// Supply this only from the target engine's actual registration state.
// An editor-only guess (e.g. usedAmmoSlots=0) is NOT an admission proof.
struct RuntimeAdmission {
    int ammoSlots = 0, usedAmmoSlots = 0;
    std::vector<std::string> occupiedAmmoNames;
    std::vector<int> occupiedWeaponIds;
};
struct Validation {
    modular::Registry registry;
    std::vector<std::string> errors;
    std::vector<std::string> warnings; // resource/VFS audit and admission status are nonfatal
    bool admissionKnown = false;
    bool admissionPassed = false;
    bool Ok() const { return errors.empty(); }
    std::string Message() const;
};
bool ValidId(const std::string& id);
bool ValidCategory(const std::string& path, bool allowRoot = false);
std::string CategoryParent(const std::string& path);
Asset Parse(const std::string& data, Kind kind);
const std::vector<std::string>& FieldNames(Kind kind);
// Explicit command-line folder wins. devkit/bin/tools installations use their
// parent game folder; a directly installed executable uses its own directory.
fs::path DiscoverRoot(const fs::path& executable, const fs::path& explicitRoot = {});

class Project {
public:
    void Load(const fs::path& root); // structural errors leave this project untouched
    const fs::path& Root() const { return root_; }
    const State& Data() const { return state_; }
    const Asset* Find(Kind kind, const std::string& id) const;
    void CheckNewId(Kind kind, const std::string& id) const;
    void Add(Kind kind, const std::string& id, const std::string& copyId = {},
        const std::optional<std::string>& category = std::nullopt, bool legacyTemplate = false);
    // Explicit legacy template is retained for old-format projects/tools.
    void AddLegacy(Kind kind, const std::string& id, const std::string& copyId = {},
        const std::optional<std::string>& category = std::nullopt) {
        Add(kind, id, copyId, category, true);
    }
    void Remove(Kind kind, const std::string& id);
    // Rename a definition and all editor-owned typed references as one undoable change.
    // Does not rename unrelated models, audio, map entities or save-game references.
    void RenameId(Kind kind, const std::string& oldId, const std::string& newId);
    void SetField(Kind kind, const std::string& id, const std::string& key, const std::string& value);
    void Allow(const std::string& weaponId, const std::string& cartridgeId, const std::string& velocity);
    void Disallow(const std::string& weaponId, const std::string& cartridgeId);
    void SetDefault(const std::string& weaponId, const std::string& cartridgeId);
    // These format-3 operations are atomic, validated and undoable.
    void SetCoefficient(const std::string& cartridgeId, const std::string& value);
    std::string SuggestedCoefficient(const std::string& cartridgeId) const;
    void PutVariant(const std::string& weaponId, const std::string& id,
        const std::string& role, const std::string& path, const std::string& when,
        const std::string& replaceId = {});
    void RemoveVariant(const std::string& weaponId, const std::string& id);
    void SetAmmoOverrides(const std::string& weaponId, const std::string& cartridgeId, const Fields& values);
    void SetSprite(Kind kind, const std::string& id, const std::string& path);
    std::vector<std::string> Categories(Kind kind) const;
    void CreateCategory(Kind kind, const std::string& path);
    void SetCategory(Kind kind, const std::string& id, const std::string& path);
    void RelocateCategory(Kind kind, const std::string& from, const std::string& to);
    void DuplicateCategory(Kind kind, const std::string& from, const std::string& to,
        const std::string& idPrefix);
    // By default, keep assets and move them to the removed category's parent.
    // Explicit subtree deletion rejects cartridges referenced by a weapon.
    void RemoveCategory(Kind kind, const std::string& path, bool deleteAssets = false);
    bool Undo();
    bool Redo();
    bool Dirty() const;
    Validation Validate(const std::optional<RuntimeAdmission>& engine = std::nullopt) const;
    // Detect backups from a power loss. Never silently overwrite active files.
    static std::vector<fs::path> PendingRecovery(const fs::path& root);
    static void RecoverInterruptedSave(const fs::path& root); // explicit restore, never auto-complete
    void Save(); // stage, validate, replace owned files, roll back on ordinary I/O failure
private:
    struct File { bool exists = false; std::string contents; };
    using Files = std::map<std::string, std::string>;
    fs::path root_;
    State state_;
    std::vector<State> undo_, redo_;
    std::map<std::string, File> baseline_;
    std::string saved_;
    Files ExportFiles() const;
    void Commit(State state);
    void CheckedCommit(State state);
};

struct Sequence { std::string name; float fps = 0; int frames = 0; };
struct ModelInfo {
    std::string name;
    std::array<float, 3> mins{}, maxs{};
    std::vector<Sequence> sequences;
};
ModelInfo ReadModel(const fs::path& path); // bounded Studio v10 metadata reader
std::vector<modular::BallisticState> Flight(const modular::Cartridge& cartridge, double velocity);
} // namespace pxwe
