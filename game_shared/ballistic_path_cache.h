// J.8 - pure SI free-flight cache. No game entities or PhysX dependency.
#pragma once
#include "ballistic_flight.h"
#include <vector>
namespace modular {
// Returns false for invalid initial physics or trajectory. Retains a bounded
// sample list until expiry, max travel, or too little remaining energy.
bool PrecomputeBallisticCurve(std::vector<BallisticState>& points,
    const BallisticState& initial,const BallisticParameters& physics,
    double maxRangeMeters,double stepSeconds=0.01,int maxSamples=1002);
// Exact sample interpolation; caller ensures elapsed time is within curve.
BallisticState SampleBallisticCurve(const std::vector<BallisticState>& points,
    double elapsedSeconds,double stepSeconds=0.01);
}
