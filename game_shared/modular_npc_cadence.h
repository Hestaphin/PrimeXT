// Server-side upper bound for modular NPC fire rate. The monster AI still
// decides WHEN to request a shot; this only refuses requests made too soon.
// This header is dependency-free so it can be tested with the firing adapter.
#pragma once
#include "modular_weapons.h"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace modular {
class NpcFireCadence {
    struct Entry { int serial; std::string weaponId; double fired, ready; };
    std::unordered_map<std::uintptr_t, Entry> shots_;
public:
    bool Ready(std::uintptr_t entity, int serial, const std::string& weaponId, double now) const {
        const auto found = shots_.find(entity);
        if (found == shots_.end()) return true;
        const auto& last = found->second;
        // Edict slot reused or level time reset: do not inherit a cooldown.
        return last.serial != serial || last.weaponId != weaponId || now < last.fired ||
               now + 1e-6 >= last.ready;
    }
    void Record(std::uintptr_t entity, int serial, const std::string& weaponId,
                double now, double rateRpm) {
        if (shots_.size() >= 4096 && !shots_.count(entity)) shots_.clear();
        shots_[entity] = {serial, weaponId, now, now + SecondsPerShot(rateRpm)};
    }
};
inline NpcFireCadence& SharedNpcFireCadence() {
    static NpcFireCadence state;
    return state;
}
} // namespace modular
