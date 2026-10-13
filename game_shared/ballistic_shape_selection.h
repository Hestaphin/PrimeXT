// Select entry geometry BEFORE attempting reverse exit. Selection is based
// on the global minimum, so candidate enumeration cannot change ambiguity.
#pragma once
#include <cmath>
#include <cstddef>
namespace modular {
struct BallisticEntryCandidate {
    int index;
    float distance;
};
struct BallisticEntrySelection {
    float distance = 0.75f;
    int index = -1;
    bool ambiguous = false;
};
inline bool EligibleBallisticEntry(const BallisticEntryCandidate &entry) {
    return entry.index >= 0 && std::isfinite(entry.distance) &&
        entry.distance >= 0.0f && entry.distance <= 0.75f;
}
inline BallisticEntrySelection SelectBallisticEntry(
    const BallisticEntryCandidate *entries, size_t count,
    float matchTolerance = 0.075f) {
    BallisticEntrySelection selection{};
    if(!entries || !std::isfinite(matchTolerance) || matchTolerance < 0.0f)
        return selection;
    for(size_t i = 0; i < count; ++i) {
        if(!EligibleBallisticEntry(entries[i])) continue;
        if(selection.index < 0 || entries[i].distance < selection.distance) {
            selection.index = entries[i].index;
            selection.distance = entries[i].distance;
        }
    }
    if(selection.index < 0) return selection;
    // Repeated references to the same identity are not another shape. The
    // caller assigns one index per actual shape before collecting entries.
    for(size_t i = 0; i < count; ++i) {
        if(EligibleBallisticEntry(entries[i]) && entries[i].index != selection.index &&
           entries[i].distance - selection.distance <= matchTolerance) {
            selection.ambiguous = true;
            break;
        }
    }
    return selection;
}
inline bool ValidBallisticEntrySelection(const BallisticEntrySelection &selection) {
    return selection.index >= 0 && !selection.ambiguous;
}
}
