#pragma once
#include <cstdint>

class CBaseEntity;
class Vector;

// Map-owned, main-thread cache for the shipped server Studio blending API.
// Results are RAW engine bone pivots; each consumer keeps its own scale rules.
bool GetServerStudioBonePose(CBaseEntity *entity, int bone, Vector &origin, Vector &angles);
std::uint64_t ServerStudioPoseRevision(CBaseEntity *entity);
std::uint64_t ServerStudioPoseGeneration();
void BeginServerStudioPoseFrame(float time);
void ClearServerStudioPoseCache();
