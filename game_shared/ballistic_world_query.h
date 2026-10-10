// J.10.6.1: validate the optional engine result before using its geometry.
// BSP intervals and physical material names are independent of trace hitgroup.
#pragma once
#include "world_ballistic_query.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>

namespace modular {
struct WorldBallisticGeometry {
    float thicknessUnits = 0;
    float entryLeadUnits = 0;
    float postExitOffsetUnits = 0;
};
inline bool ValidWorldBallisticApi(const world_ballistic_query_api_t* api) {
    return api && api->version == WORLD_BALLISTIC_QUERY_VERSION &&
        api->struct_size >= sizeof(*api) && api->query;
}
inline bool SameWorldPenetrationMaterial(const char* entry,const char* exit) {
    if (!entry || !exit || !*entry || !*exit) return false;
    while (*entry && *exit) {
        if (std::tolower(static_cast<unsigned char>(*entry++)) !=
            std::tolower(static_cast<unsigned char>(*exit++))) return false;
    }
    return !*entry && !*exit;
}
inline bool ValidateWorldBallisticGeometry(const world_ballistic_request_t& request,
    const world_ballistic_result_t& result,WorldBallisticGeometry& out) {
    if (request.struct_size < sizeof(request) || result.struct_size < sizeof(result) ||
        !result.entry_surface || !result.exit_surface ||
        !std::isfinite(request.contact_distance) || request.contact_distance < 0 ||
        request.contact_distance > 8192 ||
        !std::isfinite(request.maximum_depth) || request.maximum_depth <= 0 ||
        request.maximum_depth > 96 ||
        !std::isfinite(request.preferred_offset) || request.preferred_offset <= 0 ||
        request.preferred_offset > 0.5f ||
        !std::isfinite(result.entry_distance) || result.entry_distance <= 0 ||
        !std::isfinite(result.exit_distance) || !std::isfinite(result.clear_end_distance) ||
        std::abs(result.entry_distance-request.contact_distance) > 0.25f ||
        result.exit_distance <= std::max(result.entry_distance,request.contact_distance) ||
        result.exit_distance-request.contact_distance > request.maximum_depth ||
        result.clear_end_distance <= result.exit_distance ||
        result.clear_end_distance > request.contact_distance + request.maximum_depth +
            request.preferred_offset + 0.001f)
        return false;
    double directionNorm=0,entryNorm=0,exitNorm=0,entryDot=0,exitDot=0;
    for (int i=0;i<3;++i) {
        if (!std::isfinite(request.start[i]) || !std::isfinite(request.direction[i]) ||
            !std::isfinite(result.entry_position[i]) || !std::isfinite(result.exit_position[i]) ||
            !std::isfinite(result.entry_normal[i]) || !std::isfinite(result.exit_normal[i]))
            return false;
        directionNorm+=double(request.direction[i])*request.direction[i];
        entryNorm+=double(result.entry_normal[i])*result.entry_normal[i];
        exitNorm+=double(result.exit_normal[i])*result.exit_normal[i];
        entryDot+=double(request.direction[i])*result.entry_normal[i];
        exitDot+=double(request.direction[i])*result.exit_normal[i];
    }
    if (std::abs(directionNorm-1)>0.001 || std::abs(entryNorm-1)>0.001 ||
        std::abs(exitNorm-1)>0.001 || entryDot > -0.15 || exitDot < 0.15)
        return false;
    const double directionLength=std::sqrt(directionNorm);
    for (int i=0;i<3;++i) {
        const double direction=request.direction[i]/directionLength;
        if (std::abs(result.entry_position[i]-(request.start[i]+direction*result.entry_distance))>0.01 ||
            std::abs(result.exit_position[i]-(request.start[i]+direction*result.exit_distance))>0.01)
            return false;
    }
    WorldBallisticGeometry geometry{};
    geometry.thicknessUnits=result.exit_distance-result.entry_distance;
    geometry.entryLeadUnits=std::max(0.0f,result.entry_distance-request.contact_distance);
    // Remain strictly inside the FIRST clear interval. Even a second wall
    // entirely contained in the old 0.5-unit offset cannot be skipped.
    geometry.postExitOffsetUnits=std::min(request.preferred_offset,
        (result.clear_end_distance-result.exit_distance)*0.5f);
    if (geometry.thicknessUnits <= 0.001f || geometry.postExitOffsetUnits <= 0.001f)
        return false;
    out=geometry;
    return true;
}
}
