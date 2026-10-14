// J.8 - pure SI free-flight cache. No game entities or PhysX dependency.
#pragma once
#include "ballistic_flight.h"
#include <vector>
namespace modular {
// Returns false for invalid initial physics or trajectory. Retains a bounded
// sample list until expiry, max travel, or too little remaining energy.
bool PrecomputeBallisticCurve(std::vector<BallisticState>& points,
    const BallisticState& initial,const BallisticParameters& physics,
    double maxRangeMeters,double stepSeconds=0.01,int maxSamples=1002,
    bool exactDragDisplacement=false);
// Exact endpoints; caller ensures elapsed time is within curve. The optional
// Hermite position interpolation uses cached endpoint velocities for 20 ms
// curves. Linear velocity/range interpolation keeps energy/range bounded.
BallisticState SampleBallisticCurve(const std::vector<BallisticState>& points,
    double elapsedSeconds,double stepSeconds=0.01,bool curvedPosition=false);
}
