#ifndef STUDIO_POSE_PARAMETERS_H
#define STUDIO_POSE_PARAMETERS_H

// CPU inputs shared by drawing and collision; no render-list or GPU state.
#include "cl_entity.h"
#include "bs_defs.h"
#include <cmath>

inline float StudioPoseInterpolant(const cl_entity_t& entity, float now, bool interpolate = true)
{
    if (!interpolate || entity.curstate.animtime < entity.latched.prevanimtime + 0.01f)
        return 1.0f;
    return bound(0.0f, (now-entity.curstate.animtime)/0.1f, 2.0f);
}

inline void StudioPoseControls(const entity_state_t& state, float values[12])
{
    for (int i=0; i<3; ++i) {
        values[i] = state.vuser1[i]; values[i+3] = state.vuser2[i];
        values[i+6] = state.vuser3[i]; values[i+9] = state.vuser4[i];
    }
}

inline void StudioPreparePoseParameters(CStudioBoneSetup& setup, const cl_entity_t& entity,
    float interpolant, float parameters[MAXSTUDIOPOSEPARAM])
{
    setup.CalcDefaultPoseParameters(parameters);
    if (setup.CountPoseParameters() == 0) {
        for (int i=0; i<2; ++i)
            parameters[i] = (entity.curstate.blending[i]*interpolant +
                entity.latched.prevblending[i]*(1.0f-interpolant))/255.0f;
        return;
    }
    float current[12], previous[12];
    StudioPoseControls(entity.curstate, current);
    StudioPoseControls(entity.prevstate, previous);
    const int count = Q_min(12, Q_min(MAXSTUDIOPOSEPARAM, setup.CountPoseParameters()));
    for (int i=0; i<count; ++i)
        parameters[i] = current[i]*interpolant + previous[i]*(1.0f-interpolant);
}

inline void StudioPrepareControllers(const studiohdr_t* header, const cl_entity_t& entity,
    float interpolant, byte controllers[MAXSTUDIOCONTROLLERS])
{
    memset(controllers, 0, MAXSTUDIOCONTROLLERS);
    const auto* bindings = reinterpret_cast<const mstudiobonecontroller_t*>(
        reinterpret_cast<const byte*>(header)+header->bonecontrollerindex);
    for (int j=0; j<header->numbonecontrollers; ++j) {
        const int i = bindings[j].index;
        if (i<0 || i>3) continue; // mouth is passed separately
        int current = entity.curstate.controller[i], previous = entity.latched.prevcontroller[i];
        float value;
        if ((bindings[j].type & STUDIO_RLOOP) && abs(current-previous)>128) {
            current = (current+128)%256; previous = (previous+128)%256;
            value = current*interpolant + previous*(1.0f-interpolant)-128;
            value = std::fmod(value,256.0f);
            if (value<0) value+=256.0f;
        } else value = current*interpolant + previous*(1.0f-interpolant);
        controllers[i] = static_cast<byte>(bound(0, Q_rint(value), 255));
    }
}

#endif
