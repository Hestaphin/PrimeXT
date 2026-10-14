#pragma once

// Exact comparison, not a hash or motion tolerance: a changed pose must never
// leave an old gameplay collider active. Scratch keys retain their allocation.
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

namespace posecache
{
struct Key
{
    std::vector<std::uint64_t> words;
    void Clear() { words.clear(); }
    void Add(std::uint64_t value) { words.push_back(value); }
    void Pointer(const void *value) { Add(reinterpret_cast<std::uintptr_t>(value)); }
    void Float(float value)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        Add(bits);
    }
    template<class V> void Vector(const V &value)
    { Float(value.x); Float(value.y); Float(value.z); }
};

class Stamp
{
public:
    bool Matches(const Key &key) const { return m_valid && m_key.words == key.words; }
    void Commit(const Key &key) { m_key = key; m_valid = true; }
    void Invalidate() { m_valid = false; }
private:
    Key m_key;
    bool m_valid = false;
};

constexpr std::size_t kMeshBufferBudget = 16 * 1024 * 1024;
// Map entries supply mesh and touched fields. The current entry is kept alive
// while older payloads are evicted; an oversized mesh remains query-owned.
template<class Map>
bool MakeRoom(Map &entries, int keep, std::size_t incoming)
{
    if(incoming > kMeshBufferBudget) return false;
    std::size_t total = 0;
    for(const auto &entry : entries)
        if(entry.first != keep && entry.second.mesh) total += entry.second.mesh->GetMeshMemorySize();
    while(total > kMeshBufferBudget - incoming)
    {
        auto oldest = entries.end();
        for(auto it = entries.begin(); it != entries.end(); ++it)
            if(it->first != keep && it->second.mesh &&
                (oldest == entries.end() || it->second.touched < oldest->second.touched)) oldest = it;
        if(oldest == entries.end()) return false;
        total -= oldest->second.mesh->GetMeshMemorySize();
        entries.erase(oldest);
    }
    return true;
}
}
