#pragma once

#include <cmath>
#include <cstddef>

namespace performance
{
// The caller also runs game/render work. Tiny 100 Hz scenes should not fan
// each task chain out across every logical CPU. Explicit overrides allow
// profiling larger scenes; zero uses PhysX's synchronous dispatcher.
inline unsigned ResolvePhysXWorkers(float requested, unsigned hardwareThreads)
{
    const unsigned available = hardwareThreads ? hardwareThreads : 1;
    const unsigned automatic = available <= 2 ? 0 : (available <= 4 ? 1 : 2);
    if(!std::isfinite(requested) || requested < 0) return automatic;
    const unsigned limit = available < 32 ? available : 32;
    return requested >= float(limit) ? limit : static_cast<unsigned>(requested);
}

// PhysX scratch storage is temporary workspace, not a contact/body limit.
// Exhausting it falls back to the SDK allocator, preserving all constraints.
constexpr std::size_t kPhysXScratchBytes = 256 * 1024;
static_assert(kPhysXScratchBytes % (16 * 1024) == 0, "PhysX requires 16 KiB blocks");
}
