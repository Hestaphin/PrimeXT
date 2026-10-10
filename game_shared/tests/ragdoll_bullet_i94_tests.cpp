#include "ragdoll_bullet_policy.h"
#include <cassert>
#include <cstdio>

int main()
{
    using namespace ragdollquery;
    // Only purpose-specific weapon traces suppress obsolete hitboxes.
    assert(SkipStudioBulletHitbox(true, true, true, true));
    assert(!SkipStudioBulletHitbox(false, true, true, true)); // movement
    assert(!SkipStudioBulletHitbox(true, true, true, false)); // no active .phys
    assert(!SkipStudioBulletHitbox(true, true, false, true)); // normal character
    assert(!SkipStudioBulletHitbox(true, false, true, true)); // not a character

    // A displaced arm remains a bullet candidate even if it lies outside
    // the original GoldSrc entity hull.
    const float rayLo[] = {98, -2, -2}, rayHi[] = {102, 2, 2};
    const float armLo[] = {99, -1, -1}, armHi[] = {101, 1, 1};
    const float oldEntityLo[] = {-16, -16, -16}, oldEntityHi[] = {16, 16, 16};
    assert(OverlapWorldBounds(rayLo, rayHi, armLo, armHi));
    assert(!OverlapWorldBounds(rayLo, rayHi, oldEntityLo, oldEntityHi));

    // Rotated corpse: physics bounds are WORLD-axis AABB, not a local
    // AABB rotated a second time through entity angles.
    const float worldLo[] = {85, -8, 2}, worldHi[] = {130, 7, 24};
    assert(OverlapWorldBounds(rayLo, rayHi, worldLo, worldHi));
    const float otherLo[] = {180, -8, 2}, otherHi[] = {215, 7, 24};
    assert(!OverlapWorldBounds(rayLo, rayHi, otherLo, otherHi));
    std::puts("I.9.4 partial/full ragdoll bullet ownership and world-bounds policy: PASS");
}
