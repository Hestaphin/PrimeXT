// J.9 - target-only exit geometry. The usual trace still chooses the entry;
// this file MUST NOT invent a collision representation to replace that trace.
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "physic.h"
#include "modular_ballistic_exit.h"
#include "ballistic_exit_geometry.h"
#include <cmath>

namespace {
constexpr float kSurfaceOffset = 0.5f;

bool Valid(const Vector &p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
}

bool FindModularBallisticExit(CBaseEntity *target, const TraceResult &entry,
    int ragdollBodyIndex, const Vector &forward, float depth, BallisticExitCandidate &out) {
    if (!target || !target->pev || !Valid(entry.vecEndPos) || !Valid(forward) ||
        !std::isfinite(depth) || depth <= kSurfaceOffset || depth > 96.0f ||
        !UTIL_IsValidEntity(entry.pHit) || entry.pHit != target->edict()) return false;
    const Vector direction = forward.Normalize();
    if (direction == g_vecZero || !std::isfinite(entry.flFraction) ||
        entry.fStartSolid || entry.fAllSolid) return false;

    Vector candidate;
    Vector normal;
    bool found = false;
    // A ragdoll must exit the SAME .phys body that was hit, never the next
    // forearm or a coincident world/animated representation.
    if (WorldPhysic && WorldPhysic->IsRagdoll(target)) {
        if (ragdollBodyIndex < 0) return false;
        found = WorldPhysic->TraceBallisticPhysicsExit(target,ragdollBodyIndex,
            entry.vecEndPos,direction,depth,candidate,normal);
    } else if (!(target->pev->flags & (FL_CLIENT | FL_MONSTER)) &&
               target->pev->solid == SOLID_CUSTOM && WorldPhysic &&
               WorldPhysic->HasPhysicsAsset(target)) {
        // Primitive-only .phys shapes may be absent from the engine's model
        // trace. The PhysX query validates the identical shape at entry/exit.
        found = WorldPhysic->TraceBallisticPhysicsExit(target,-1,
            entry.vecEndPos,direction,depth,candidate,normal);
    } else {
        // Xash's TRACE_MODEL traces one named entity, including the CURRENT
        // animated Studio hitboxes and the _cp-enabled custom collision model.
        // The reverse hit must belong to that entity and have the correct
        // outward-facing exit normal. It never accepts a scene-wide ray hit.
        TraceResult reverse{};
        const int oldFlags = gpGlobals->trace_flags;
        SetBits(gpGlobals->trace_flags,FTRACE_MATERIAL_TRACE | FTRACE_BULLET_PHYSICS);
        UTIL_TraceModel(entry.vecEndPos+direction*depth,
            entry.vecEndPos+direction*kSurfaceOffset,0,target->edict(),&reverse);
        gpGlobals->trace_flags = oldFlags;
        if (!reverse.fStartSolid && !reverse.fAllSolid && reverse.flFraction < 1.0f &&
            reverse.pHit == target->edict() && Valid(reverse.vecEndPos) &&
            Valid(reverse.vecPlaneNormal) &&
            (!(target->pev->flags & (FL_CLIENT | FL_MONSTER)) ||
             (entry.iHitgroup != 0 && entry.iHitgroup == reverse.iHitgroup))) {
            candidate = reverse.vecEndPos;
            normal = reverse.vecPlaneNormal;
            found = true;
        }
    }
    if (!found || !Valid(candidate) || !Valid(normal)) return false;
    const Vector displacement = candidate-entry.vecEndPos;
    const float thickness = DotProduct(displacement,direction);
    const float sideways = (displacement-direction*thickness).Length();
    if (!modular::ValidBallisticExit(depth,thickness,sideways,DotProduct(normal,direction))) return false;

    // A narrow world/brush query between the surfaces prevents exit-through-
    // wall when the model overlaps another physical barrier. This deliberately
    // ignores the penetrated entity and characters, not world brushes.
    TraceResult blocked{};
    const int oldFlags = gpGlobals->trace_flags;
    SetBits(gpGlobals->trace_flags,FTRACE_BULLET_PHYSICS);
    UTIL_TraceLine(entry.vecEndPos+direction*kSurfaceOffset,
        candidate+direction*kSurfaceOffset,ignore_monsters,target->edict(),&blocked);
    gpGlobals->trace_flags = oldFlags;
    if (blocked.fStartSolid || blocked.fAllSolid || blocked.flFraction < 1.0f) return false;
    out.position = candidate;
    out.thicknessUnits = thickness;
    return true;
}
