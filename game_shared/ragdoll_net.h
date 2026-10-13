#pragma once

#include <cstdint>

namespace ragdollnet
{
constexpr std::uint8_t kProtocolVersion = 1;

// Full snapshots are self-contained. The clear bit terminates the current
// ragdoll presentation state, while sleeping lets the server reduce refresh
// rate without changing the pose format.
enum SnapshotFlags : std::uint8_t
{
    SnapshotSleeping = 1 << 0,
    SnapshotClear = 1 << 1,
    SnapshotFrozen = 1 << 2,
};

// Relative-to-root body translations are quantized to 1/8 engine unit.
// This gives a +/-4095.875 unit articulated span, far beyond normal character
// proportions, while keeping every sparse body record to 15 bytes.
constexpr float kRelativePositionScale = 8.0f;
constexpr float kInverseRelativePositionScale = 1.0f / kRelativePositionScale;
constexpr float kQuaternionScale = 32767.0f;
constexpr float kInverseQuaternionScale = 1.0f / kQuaternionScale;

constexpr int kHeaderBytes = 30;      // entity/version/flags/seq/root/count + root transform
constexpr int kBoneRecordBytes = 15;  // bone + 3 shorts + 4 shorts
constexpr int kUserMessageLimit = 2048;

static_assert(kHeaderBytes + 127 * kBoneRecordBytes < kUserMessageLimit,
    "A full 128-bone Studio ragdoll snapshot must fit in one user message");
}
