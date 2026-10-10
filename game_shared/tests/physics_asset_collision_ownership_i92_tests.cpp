// I.9.2 regression: game-requested, bodygroup-selected and articulated-owner
// states are independent, regardless of which event arrived most recently.
#include "../physics_asset_collision_ownership.h"
#include <cassert>

using physicsasset::ResolveRootCollision;
using physicsasset::RootCollisionAction;

static RootCollisionAction choose(bool request, bool hasGeometry, bool articulated,
                                  bool restoring, bool allowEnable=true)
{
    return ResolveRootCollision(request,hasGeometry,articulated,restoring,allowEnable);
}
int main()
{
    // All four ownership conditions are required to activate a root shape.
    assert(choose(true,true,false,false)==RootCollisionAction::Enable);
    assert(choose(false,true,false,false)==RootCollisionAction::Disable);
    assert(choose(true,false,false,false)==RootCollisionAction::Disable);
    assert(choose(true,true,true,false)==RootCollisionAction::Disable);
    assert(choose(true,true,false,true)==RootCollisionAction::Disable);

    // While a ragdoll owns the entity, every bodygroup selection is silent.
    for (int body=0;body<2;++body)
        assert(choose(true,body!=0,true,true)==RootCollisionAction::Disable);
    assert(choose(true,true,true,false)==RootCollisionAction::Disable);

    // A gameplay toggle to on while ragdolled only records future intent.
    assert(choose(false,true,true,false)==RootCollisionAction::Disable);
    assert(choose(true,true,true,false)==RootCollisionAction::Disable);

    // Exit must not resurrect the root before the saved flags are restored.
    assert(choose(true,true,false,true)==RootCollisionAction::Disable);
    assert(choose(true,true,false,false)==RootCollisionAction::Enable);
    assert(choose(true,false,false,false)==RootCollisionAction::Disable);
    assert(choose(false,true,false,false)==RootCollisionAction::Disable);

    // Don't overwrite saved legacy simulation/query flags if no new
    // explicit enable was requested after restoring legacy actors.
    assert(choose(true,true,false,false,false)==RootCollisionAction::Preserve);
    assert(choose(true,true,true,false,false)==RootCollisionAction::Disable);
    assert(choose(true,true,false,true,false)==RootCollisionAction::Disable);
    return 0;
}
