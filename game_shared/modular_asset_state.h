#pragma once
#include "modular_asset_selection.h"
#include <cmath>

namespace modular {
// Exact integer fragments within the existing signed float delta quantizers:
// fuser1 (scale 1000): 11 bits; fuser2/3 (scale 128): 14 bits each.
inline void EncodeAssetState(uint32_t seed, int action, float& first, float& second, float& third) {
    first = float(seed & 2047u);
    second = float((seed >> 11) & 16383u);
    third = float((seed >> 25) | (uint32_t(ValidAssetAction(action) ? action : 0) << 7));
}
inline bool DecodeAssetState(float first, float second, float third, uint32_t& seed, int& action) {
    const float values[] = { first, second, third };
    const int limits[] = { 2047, 16383, 1023 };
    unsigned parts[3]{};
    for (int i=0; i<3; ++i) {
        if (!std::isfinite(values[i]) || values[i] < 0 || values[i] > limits[i] ||
            std::abs(values[i] - std::round(values[i])) > .01f) { seed=0; action=0; return false; }
        parts[i] = unsigned(std::round(values[i]));
    }
    action = int(parts[2] >> 7);
    if (!ValidAssetAction(action)) { seed=0; action=0; return false; }
    seed = parts[0] | (parts[1] << 11) | ((parts[2] & 127u) << 25);
    return true;
}
} // namespace modular
