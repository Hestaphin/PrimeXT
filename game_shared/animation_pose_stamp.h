#pragma once

#include <array>
#include <cstdint>

namespace performance
{
// GET_BONE_POSITION's Studio inputs plus the frame used for scaled pivots.
// Cache only within one server timestamp: another timestamp always samples
// the engine, including custom blending callbacks driven by server time.
struct AnimationPoseStamp
{
    const void *model = nullptr;
    int modelIndex = 0, sequence = 0;
    float frame = 0, time = 0, rigidScale = 1;
    std::array<float, 3> origin{}, angles{}, ownerOrigin{};
    std::array<std::uint8_t, 4> controller{};
    std::array<std::uint8_t, 2> blending{};
    bool valid = false;

    bool operator==(const AnimationPoseStamp &other) const
    {
        return valid && other.valid && model == other.model && modelIndex == other.modelIndex &&
            sequence == other.sequence && frame == other.frame && time == other.time && rigidScale == other.rigidScale &&
            origin == other.origin && angles == other.angles && ownerOrigin == other.ownerOrigin &&
            controller == other.controller && blending == other.blending;
    }
};

template<class Vars, class Vector>
inline AnimationPoseStamp MakeAnimationPoseStamp(const void *model, const Vars &vars,
    const Vector &ownerOrigin, float time, float rigidScale)
{
    AnimationPoseStamp stamp;
    stamp.model = model;
    stamp.modelIndex = vars.modelindex;
    stamp.sequence = vars.sequence;
    stamp.frame = vars.frame;
    stamp.time = time;
    stamp.rigidScale = rigidScale;
    for(int i = 0; i < 3; ++i)
    {
        stamp.origin[i] = vars.origin[i];
        stamp.angles[i] = vars.angles[i];
        stamp.ownerOrigin[i] = ownerOrigin[i];
    }
    for(int i = 0; i < 4; ++i) stamp.controller[i] = vars.controller[i];
    for(int i = 0; i < 2; ++i) stamp.blending[i] = vars.blending[i];
    stamp.valid = model != nullptr;
    return stamp;
}
}
