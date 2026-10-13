#include "physics_collision_pose_freshness.h"
#include <cassert>
#include <limits>

using physicsasset::CollisionPoseState;
using physicsasset::CollisionPoseStamp;
using physicsasset::CollisionPoseFresh;

int main()
{
    CollisionPoseStamp cache;
    CollisionPoseState state;
    state.sequence = 2;
    state.frame = 11.0f;
    state.frameRate = 0.0f;
    state.animTime = 1.0f;
    state.renderOrigin[0] = state.stateOrigin[0] = 100;
    state.renderAngles[1] = state.stateAngles[1] = 90;
    state.poseControls[1] = 0.25f;
    state.scale = 2.0f;
    state.staticScale = {2,2,2};
    state.blending = {1,2,3,4};
    state.controllers = {5,6,7,8};
    state.mouth = 1;
    cache.state = state;
    cache.sampleTime = 100.0f;
    assert(!CollisionPoseFresh(cache,state,100.0f)); // not yet rendered
    cache.valid = true;
    assert(CollisionPoseFresh(cache,state,100.0f));
    assert(CollisionPoseFresh(cache,state,100.04f)); // short static snapshot window
    assert(!CollisionPoseFresh(cache,state,100.1f)); // offscreen, too old
    assert(!CollisionPoseFresh(cache,state,99.99f)); // backward time
    assert(!CollisionPoseFresh(cache,state,std::numeric_limits<float>::infinity()));
    assert(!CollisionPoseFresh(cache,state,std::numeric_limits<float>::quiet_NaN()));
    cache.sampleTime = std::numeric_limits<float>::quiet_NaN();
    assert(!CollisionPoseFresh(cache,state,100.0f));
    cache.sampleTime = 100.0f;

    auto changed=state;
    changed.frame=12.0f;                 // same animation sequence, new frame
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.animTime=2.0f; // new animation base time
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.prevAnimTime=0.5f;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.oldBlending[0]=7;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.oldControllers[0]=7;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.frameRate=1.0f;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.renderOrigin[0]=101.0f;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.stateOrigin[0]=101.0f; // network update before next render
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.renderAngles[1]=95.0f;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.stateAngles[1]=95.0f;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.scale=1.0f;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.staticScale[1]=1.0f;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.scaleFlags=8;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.poseControls[1]=0.33f;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.controllers[1]=9;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.blending[2]=9;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.gaitSequence=3;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.gaitFrame=3.0f;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.mouth=9;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.effects=1;
    assert(!CollisionPoseFresh(cache,changed,100.0f));
    changed=state; changed.renderFx=1;
    assert(!CollisionPoseFresh(cache,changed,100.0f));

    // Even if packets do not change the source state, an animated pose or
    // an interpolated physical part is invalid on the next render-time tick.
    changed=state; changed.frameRate=1.0f; changed.timeDependent=true;
    cache.state=changed;
    assert(CollisionPoseFresh(cache,changed,100.0005f));
    assert(!CollisionPoseFresh(cache,changed,100.02f));
    changed=state; changed.timeDependent=true; // physical part, same sequence
    cache.state=changed;
    assert(!CollisionPoseFresh(cache,changed,100.02f));
    return 0;
}
