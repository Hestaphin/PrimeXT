#include "ballistic_world_query.h"
#include <cassert>
#include <limits>
#include <iostream>

static int unavailable(const world_ballistic_request_t*,world_ballistic_result_t*) { return 0; }
int main() {
    using namespace modular;
    world_ballistic_query_api_t api{WORLD_BALLISTIC_QUERY_VERSION,sizeof(api),unavailable};
    assert(ValidWorldBallisticApi(&api));
    assert(!ValidWorldBallisticApi(nullptr));
    api.version=2; assert(!ValidWorldBallisticApi(&api)); api.version=1;
    api.struct_size=8; assert(!ValidWorldBallisticApi(&api)); api.struct_size=sizeof(api);
    api.query=nullptr; assert(!ValidWorldBallisticApi(&api));
    world_ballistic_request_t request{};
    request.struct_size=sizeof(request);
    request.start[0]=-1; request.direction[0]=1;
    request.contact_distance=0.96875f; request.maximum_depth=96; request.preferred_offset=0.5f;
    world_ballistic_result_t result{};
    result.struct_size=sizeof(result);
    result.entry_distance=1; result.exit_distance=3.08f; result.clear_end_distance=3.18f;
    result.entry_position[0]=0; result.exit_position[0]=2.08f;
    result.entry_normal[0]=-1; result.exit_normal[0]=1;
    // Opaque identities only: this policy never dereferences engine surfaces.
    result.entry_surface=reinterpret_cast<msurface_s*>(uintptr_t(1));
    result.exit_surface=reinterpret_cast<msurface_s*>(uintptr_t(2));
    WorldBallisticGeometry geometry{};
    assert(ValidateWorldBallisticGeometry(request,result,geometry));
    assert(std::abs(geometry.thicknessUnits-2.08f)<1e-6);
    assert(std::abs(geometry.entryLeadUnits-0.03125f)<1e-6);
    assert(std::abs(geometry.postExitOffsetUnits-0.05f)<1e-6);
    assert(result.exit_position[0]+geometry.postExitOffsetUnits<2.18f);
    result.clear_end_distance=95;
    assert(ValidateWorldBallisticGeometry(request,result,geometry));
    assert(geometry.postExitOffsetUnits==0.5f);
    const auto valid=result;
    result.clear_end_distance=result.exit_distance; assert(!ValidateWorldBallisticGeometry(request,result,geometry));
    result=valid; result.entry_surface=nullptr; assert(!ValidateWorldBallisticGeometry(request,result,geometry));
    result=valid; result.exit_distance=std::numeric_limits<float>::quiet_NaN(); assert(!ValidateWorldBallisticGeometry(request,result,geometry));
    result=valid; result.exit_normal[0]=-1; assert(!ValidateWorldBallisticGeometry(request,result,geometry));
    result=valid; result.exit_position[0]=4; assert(!ValidateWorldBallisticGeometry(request,result,geometry));
    result=valid; result.entry_distance=2; assert(!ValidateWorldBallisticGeometry(request,result,geometry));
    result=valid; result.struct_size=8; assert(!ValidateWorldBallisticGeometry(request,result,geometry));
    assert(SameWorldPenetrationMaterial("glass","GLASS"));
    assert(!SameWorldPenetrationMaterial("glass","concrete"));
    assert(!SameWorldPenetrationMaterial("glass","glass_extra"));
    assert(!SameWorldPenetrationMaterial(nullptr,"glass"));
    assert(!SameWorldPenetrationMaterial("",""));
    std::cout<<"PASS J.10.6.1: API negotiation, exact geometry, safe exit offsets and actual-material identity\n";
}
