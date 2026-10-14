// K.11.4.1.6: curve math follows simulated time, independently of server FPS.
#pragma once
#include "ballistic_flight.h"
#include <algorithm>
#include <cmath>

namespace modular {
constexpr double kBallisticCoarseStep = 0.02;
constexpr double kBallisticFineStep = 0.01;
constexpr double kBallisticSweepStep = 0.02;
constexpr double kBallisticLifetime = 10.0;
constexpr double kBallisticMaxCoarseDrag = 0.05;

struct BallisticCurvePolicy {
    double step = kBallisticCoarseStep;
    bool accuracyFallback = false;
    int SampleLimit() const {
        return int(std::ceil(kBallisticLifetime / step)) + 2;
    }
};

inline BallisticCurvePolicy ChooseBallisticCurvePolicy(const BallisticState& state,
    const BallisticParameters& physics, double requestedHz) {
    BallisticCurvePolicy policy;
    // Only the tested 50/100 Hz rates are supported; invalid settings use 50.
    if (std::isfinite(requestedHz) && requestedHz >= 75.0) {
        policy.step = kBallisticFineStep;
        return policy;
    }
    // Gravity can add at most g*time to speed; drag only reduces speed.
    // Limit k*speed*dt to 0.05 so
    // unusual cartridge dimensions/mass cannot magnify coarse-step errors.
    const double remaining = (std::max)(0.0, kBallisticLifetime - state.seconds);
    const auto speed = [](BallisticVec3 v) {
        return std::hypot(std::hypot(v.x, v.y), v.z);
    };
    const double maximumSpeed = speed(state.velocity) + physics.gravity * remaining;
    const double area = 3.141592653589793 * physics.diameterM * physics.diameterM * 0.25;
    const double drag = 0.5 * physics.airDensity * physics.dragCoefficient * area / physics.massKg;
    const double coarseDrag = drag * maximumSpeed * kBallisticCoarseStep;
    if (!std::isfinite(coarseDrag) || coarseDrag > kBallisticMaxCoarseDrag) {
        policy.step = kBallisticFineStep;
        policy.accuracyFallback = true;
    }
    return policy;
}
} // namespace modular
