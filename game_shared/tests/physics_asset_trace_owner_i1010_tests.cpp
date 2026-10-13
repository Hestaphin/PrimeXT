// I.10.10: root and primitive-child movement collision ownership regression.
#include "../physics_asset_trace_owner.h"
#include <cassert>
#include <iostream>

int main()
{
    using physicsasset::SelectMovementSweepOwners;
    const auto noActor = SelectMovementSweepOwners(false, true, true, true);
    assert(!noActor.Any());
    const auto legacy = SelectMovementSweepOwners(true, true, true, false);
    assert(legacy.Any() && legacy.root && !legacy.children);
    const auto compound = SelectMovementSweepOwners(true, true, true, true);
    assert(compound.Any() && compound.root && compound.children);
    // This is the I.10.10 bug: the old root-shape-count guard skipped
    // a completely valid hinged child actor with its own live shapes.
    const auto childOnly = SelectMovementSweepOwners(true, false, false, true);
    assert(childOnly.Any() && !childOnly.root && childOnly.children);
    const auto disabledRoot = SelectMovementSweepOwners(true, true, false, true);
    assert(disabledRoot.Any() && !disabledRoot.root && disabledRoot.children);
    const auto inactive = SelectMovementSweepOwners(true, true, false, false);
    assert(!inactive.Any());
    const auto empty = SelectMovementSweepOwners(true, false, false, false);
    assert(!empty.Any());
    std::cout << "I.10.10 root/child movement sweep ownership PASS\n";
}
