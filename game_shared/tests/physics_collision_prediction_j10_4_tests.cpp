// Regression for J.10.4: render-only pose stamps do not cover prediction time.
#include "physics_collision_pose_freshness.h"
#include <cassert>
#include <iostream>
using namespace physicsasset;
int main() {
    CollisionPoseState animated{};
    animated.sequence = 7;
    animated.timeDependent = true;
    CollisionPoseStamp lastRender{};
    lastRender.valid = true;
    lastRender.state = animated;
    lastRender.sampleTime = 10.0f;
    assert(CollisionPoseFresh(lastRender, animated, 10.0f));
    // At 60 Hz, a newly received packet is predicted before the next draw.
    assert(!CollisionPoseFresh(lastRender, animated, 10.0f + 1.0f / 60.0f));
    CollisionPoseStamp prediction{};
    prediction.valid = true;
    prediction.state = animated;
    prediction.sampleTime = 10.0f + 1.0f / 60.0f;
    assert(CollisionPoseFresh(prediction, animated, prediction.sampleTime));
    // Changing the animation state or invalidating a physical snapshot
    // must not accept a previous prediction evaluation.
    animated.sequence = 8;
    assert(!CollisionPoseFresh(prediction, animated, prediction.sampleTime));
    animated.sequence = 7;
    prediction.valid = false;
    assert(!CollisionPoseFresh(prediction, animated, prediction.sampleTime));
    std::cout << "J.10.4 pose timestamp policy: PASS\n";
}
