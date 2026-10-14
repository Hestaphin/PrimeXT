#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "studio.h"
#include "studio_pose_cache.h"
#include "performance_profile.h"

#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <vector>

namespace
{
constexpr std::size_t kPoseEntryLimit = 128;
struct PoseInput
{
    const void *entity = nullptr, *edict = nullptr, *model = nullptr, *header = nullptr;
    int serial = 0, modelIndex = 0, bones = 0, sequence = 0, gait = 0, body = 0, skin = 0;
    std::uint32_t modelCRC = 0;
    std::array<char, 64> name{};
    std::array<byte, 6> controls{};
    std::array<float, 20> values{};
    bool operator==(const PoseInput &other) const
    {
        return entity == other.entity && edict == other.edict && model == other.model && header == other.header &&
            serial == other.serial && modelIndex == other.modelIndex && bones == other.bones &&
            sequence == other.sequence && gait == other.gait && body == other.body && skin == other.skin &&
            modelCRC == other.modelCRC && name == other.name && controls == other.controls &&
            std::memcmp(values.data(), other.values.data(), sizeof(values)) == 0;
    }
};
struct BonePose { Vector origin, angles; bool valid = false; };
struct PoseEntry
{
    PoseInput input;
    std::vector<BonePose> bones;
    std::uint64_t revision = 0, touched = 0;
};
std::map<int, PoseEntry> g_poses;
std::uint64_t g_revision = 0, g_generation = 1, g_touch = 0;
float g_previousTime = 0;
bool g_hasTime = false;

PoseEntry *FindPose(CBaseEntity *entity)
{
    if(!entity || !entity->pev) return nullptr;
    edict_t *edict = entity->edict();
    if(!edict || edict->free) return nullptr;
    model_t *model = (model_t *)MODEL_HANDLE(entity->pev->modelindex);
    studiohdr_t *header = model && model->type == mod_studio ? (studiohdr_t *)model->cache.data : nullptr;
    if(!header || header->numbones <= 0 || header->numbones > MAXSTUDIOBONES) return nullptr;
    PoseInput input;
    input.entity = entity; input.edict = edict; input.model = model; input.header = header;
    input.serial = edict->serialnumber; input.modelIndex = entity->pev->modelindex;
    input.bones = header->numbones; input.modelCRC = model->modelCRC;
    std::memcpy(input.name.data(), model->name, input.name.size());
    input.sequence = entity->pev->sequence; input.gait = entity->pev->gaitsequence;
    input.body = entity->pev->body; input.skin = entity->pev->skin;
    std::memcpy(input.controls.data(), entity->pev->controller, 4);
    std::memcpy(input.controls.data() + 4, entity->pev->blending, 2);
    const Vector vectors[] = {entity->pev->origin, entity->pev->angles,
        entity->GetAbsOrigin(), entity->GetAbsAngles(), entity->GetScale()};
    std::size_t index = 0;
    for(const Vector &value : vectors)
    { input.values[index++] = value.x; input.values[index++] = value.y; input.values[index++] = value.z; }
    input.values[index++] = entity->pev->frame;
    input.values[index++] = entity->pev->animtime;
    input.values[index++] = entity->pev->framerate;
    input.values[index++] = entity->pev->scale;
    input.values[index++] = static_cast<float>(entity->pev->movetype);
    // Non-finite state deliberately cannot enter the cache.
    for(float value : input.values) if(!std::isfinite(value)) return nullptr;
    const int ownerIndex = entity->entindex();
    auto found = g_poses.find(ownerIndex);
    if(found == g_poses.end())
    {
        if(g_poses.size() == kPoseEntryLimit)
        {
            auto oldest = g_poses.begin();
            for(auto it = g_poses.begin(); it != g_poses.end(); ++it)
                if(it->second.touched < oldest->second.touched) oldest = it;
            g_poses.erase(oldest);
        }
        found = g_poses.emplace(ownerIndex, PoseEntry{}).first;
    }
    PoseEntry &entry = found->second;
    if(!entry.revision || !(entry.input == input))
    {
        entry.input = input;
        entry.bones.assign(input.bones, BonePose{});
        entry.revision = ++g_revision;
        if(!entry.revision) entry.revision = ++g_revision;
    }
    entry.touched = ++g_touch;
    return &entry;
}
}

bool GetServerStudioBonePose(CBaseEntity *entity, int bone, Vector &origin, Vector &angles)
{
    PoseEntry *entry = FindPose(entity);
    if(!entry || bone < 0 || static_cast<std::size_t>(bone) >= entry->bones.size()) return false;
    BonePose &pose = entry->bones[bone];
    if(pose.valid)
        pxperf::Get().Add(pxperf::Metric::ServerPoseCacheHits, 1);
    else
    {
        pxperf::Scope scope(pxperf::Metric::ServerBoneQuery);
        pxperf::Get().Add(pxperf::Metric::ServerPoseCacheMisses, 1);
        // The engine API returns void. Refuse an unwritten/partial result
        // rather than accidentally publishing a previously valid bone.
        pose.origin.Init(NAN, NAN, NAN); pose.angles.Init(NAN, NAN, NAN);
        GET_BONE_POSITION(entity->edict(), bone, pose.origin, pose.angles);
        const float values[] = {pose.origin.x, pose.origin.y, pose.origin.z,
            pose.angles.x, pose.angles.y, pose.angles.z};
        for(float value : values) if(!std::isfinite(value)) return false;
        pose.valid = true;
    }
    origin = pose.origin; angles = pose.angles;
    return true;
}

std::uint64_t ServerStudioPoseRevision(CBaseEntity *entity)
{
    PoseEntry *entry = FindPose(entity);
    return entry ? entry->revision : 0;
}
std::uint64_t ServerStudioPoseGeneration() { return g_generation; }
void ClearServerStudioPoseCache()
{
    g_poses.clear(); g_hasTime = false;
    ++g_generation; if(!g_generation) ++g_generation;
}
void BeginServerStudioPoseFrame(float time)
{
    if(!std::isfinite(time) || (g_hasTime && time < g_previousTime)) ClearServerStudioPoseCache();
    g_previousTime = time; g_hasTime = std::isfinite(time);
}
