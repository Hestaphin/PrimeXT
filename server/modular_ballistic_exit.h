// J.9 - same-representation exit queries for model/physics bullet penetration.
// Entry is resolved by the existing I.10.10 nearest-hit pipeline. No damage
// or physics mutations happen until a valid exit candidate is captured.
#pragma once
#include "vector.h"
class CBaseEntity;
struct gametrace_s;
struct BallisticExitCandidate {
    Vector position;
    float thicknessUnits = 0;
};
bool FindModularBallisticExit(CBaseEntity *target, const gametrace_s &entry,
    int ragdollBodyIndex, const Vector &direction, float maximumDepthUnits,
    BallisticExitCandidate &result);
