// J.10.2 regression: first BSP solid interval and exact PhysX entry selection.
#include "ballistic_world_continuity.h"
#include "ballistic_shape_selection.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace modular;
    WorldExitInterval exit{};
    // World edict 0 contains both glass at [0,2] and concrete at [20,22].
    auto twoBrushes=[](float x) { return (x<2.0f) || (x>=20.0f && x<22.0f); };
    assert(FindFirstWorldSolidExit(96.0f,twoBrushes,exit));
    assert(std::fabs(exit.outsideDistance-2.0f)<0.001f);
    // No known air boundary: never guess a far back face.
    assert(!FindFirstWorldSolidExit(96.0f,[](float) { return true; },exit));
    // Entry not inside expected geometry: fail rather than skip a thin brush.
    assert(!FindFirstWorldSolidExit(96.0f,[](float x) { return x<0.02f; },exit));
    assert(!FindFirstWorldSolidExit(96.0f,[](float) { return false; },exit));
    // A single standalone wall can be exited, subject to material verification.
    assert(FindFirstWorldSolidExit(96.0f,[](float x) { return x<8.0f; },exit));
    assert(std::fabs(exit.outsideDistance-8.0f)<0.001f);
    const BallisticEntryCandidate distinct[]={{0,0.04f},{1,0.3f}};
    auto selected=SelectBallisticEntry(distinct,2);
    assert(ValidBallisticEntrySelection(selected) && selected.index==0);
    const BallisticEntryCandidate coincident[]={{0,0.04f},{1,0.3f},{2,0.045f},{3,0.0f}};
    assert(!ValidBallisticEntrySelection(SelectBallisticEntry(coincident,4)));
    const BallisticEntryCandidate separate[]={{0,0.2f},{1,0.0f}};
    auto nearest=SelectBallisticEntry(separate,2);
    assert(ValidBallisticEntrySelection(nearest) && nearest.index==1);
    std::cout<<"PASS J.10.2: nearest world exit and unambiguous same-shape selection\n";
}
