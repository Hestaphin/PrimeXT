// J.10.5: physical-instance identities, independent of weapon definition IDs.
// These are portable value types, not engine edicts, entity indices or ammo slots.
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace modular_identity {
inline bool IsUuid(const std::string& s) {
    if (s.size() != 36 || s[8] != '-' || s[13] != '-' || s[18] != '-' || s[23] != '-') return false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) continue;
        const char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return s[14] == '4' && (s[19] == '8' || s[19] == '9' || s[19] == 'a' || s[19] == 'b');
}

// Version-4 UUID. The host's random_device must supply adequate randomness;
// identity is not a cryptographic authorization token or a proof of ownership.
inline std::string NewUuid() {
    std::random_device rng;
    std::array<unsigned char,16> bytes{};
    for (unsigned i = 0; i < 4; ++i) {
        const uint32_t word = (uint32_t(rng()) << 16) ^ uint32_t(rng());
        for (unsigned j = 0; j < 4; ++j) bytes[i*4+j] = static_cast<unsigned char>(word >> (j*8));
    }
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);
    char out[37];
    std::snprintf(out,sizeof(out),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],
        bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
    return out;
}

// Future mixed-magazine persistence schema. One cartridge ID per round in
// feed order; no runtime firing/reload logic is switched to this yet.
struct MagazineRecord {
    std::string uuid;
    std::string chamberedCartridge; // separate chamber state, if applicable
    std::vector<std::string> rounds; // [0] is next round presented by magazine
    int capacity = 0;
};
inline bool ValidMagazine(const MagazineRecord& m) {
    if (!IsUuid(m.uuid) || m.capacity < 0 || m.capacity > 512 ||
        m.rounds.size() > static_cast<size_t>(m.capacity)) return false;
    for (const auto& id : m.rounds) if (id.empty() || id.size() > 64) return false;
    return m.chamberedCartridge.size() <= 64;
}

enum class MapExitOwner { PlayerCarried, NpcCarried, WorldUnclaimed };
// Campaign policy: persist only the player's actual carried inventory.
// NPCs/world pickups remain map-owned and are discarded at a transition.
inline bool CarryAcrossLevel(MapExitOwner owner) { return owner == MapExitOwner::PlayerCarried; }
} // namespace modular_identity
