// I.10.6: Source-state and time validation for renderer-derived collision bones.
// Prediction must not use a bone merely because its Studio sequence is unchanged.
#ifndef PHYSICS_COLLISION_POSE_FRESHNESS_H
#define PHYSICS_COLLISION_POSE_FRESHNESS_H

#include <array>
#include <cmath>
#include <cstdint>

namespace physicsasset
{
struct CollisionPoseState
{
    int sequence = 0;
    int gaitSequence = 0;
    int effects = 0;
    int renderFx = 0;
    int scaleFlags = 0;
    float frame = 0.0f;
    float animTime = 0.0f;
    float prevAnimTime = 0.0f;
    float frameRate = 0.0f;
    float gaitFrame = 0.0f;
    float scale = 0.0f;
    std::array<float, 3> renderOrigin{};
    std::array<float, 3> renderAngles{};
    std::array<float, 3> stateOrigin{};
    std::array<float, 3> stateAngles{};
    std::array<float, 3> staticScale{};
    std::array<float, 12> poseControls{};
    std::array<float, 12> previousPoseControls{};
    std::array<uint8_t, 4> blending{};
    std::array<uint8_t, 4> controllers{};
    std::array<uint8_t, 4> oldControllers{};
    std::array<uint8_t, 2> oldBlending{};
    uint8_t mouth = 0;
    bool timeDependent = false;
};

struct CollisionPoseStamp
{
    CollisionPoseState state{};
    float sampleTime = 0.0f;
    bool valid = false;
};

inline bool CollisionPoseStateEqual(const CollisionPoseState &a, const CollisionPoseState &b)
{
    return a.sequence == b.sequence && a.gaitSequence == b.gaitSequence &&
        a.effects == b.effects && a.renderFx == b.renderFx && a.scaleFlags == b.scaleFlags &&
        a.frame == b.frame && a.animTime == b.animTime &&
        a.prevAnimTime == b.prevAnimTime && a.frameRate == b.frameRate &&
        a.gaitFrame == b.gaitFrame && a.scale == b.scale &&
        a.renderOrigin == b.renderOrigin && a.renderAngles == b.renderAngles &&
        a.stateOrigin == b.stateOrigin && a.stateAngles == b.stateAngles &&
        a.staticScale == b.staticScale && a.poseControls == b.poseControls &&
        a.previousPoseControls == b.previousPoseControls &&
        a.blending == b.blending && a.controllers == b.controllers &&
        a.oldControllers == b.oldControllers && a.oldBlending == b.oldBlending &&
        a.mouth == b.mouth && a.timeDependent == b.timeDependent;
}

inline bool CollisionPoseFresh(const CollisionPoseStamp &stamp, const CollisionPoseState &current,
    float queryTime)
{
    if (!stamp.valid || !std::isfinite(queryTime) || !std::isfinite(stamp.sampleTime) ||
        queryTime < stamp.sampleTime || !CollisionPoseStateEqual(stamp.state, current))
        return false;

    // A moving animation, externally posed part, or ragdoll can advance
    // between render frames without any change to its sequence or network state.
    // In that case only a bone generated for the same client time is safe.
    // Even a static pose has a short validity window: offscreen models may
    // stop rendering while an authoritative server update is on its way.
    const float maxAge = current.timeDependent ? 0.001f : 0.05f;
    return queryTime - stamp.sampleTime <= maxAge;
}
} // namespace physicsasset

#endif
