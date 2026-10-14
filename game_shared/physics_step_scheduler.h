#pragma once
// Fixed simulated-time steps, independent of display/server call frequency.
// Overload discards time explicitly; it never increases the solver timestep or
// leaves whole-step debt to trigger another expensive catch-up next frame.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace physicsasset
{
constexpr int kDefaultPhysicsHz = 60;
constexpr int kDefaultPhysicsMaxSteps = 4;
constexpr int kHardPhysicsMaxSteps = 8;
constexpr double kMaximumPhysicsFrameSeconds = 0.25;

inline int ResolvePhysicsHz(float value)
{
    return value == 30.0f ? 30 : value == 100.0f ? 100 : kDefaultPhysicsHz;
}
inline int ResolvePhysicsMaxSteps(float value)
{
    if(!std::isfinite(value)) return kDefaultPhysicsMaxSteps;
    // Clamp before conversion, including enormous finite cvar values.
    return static_cast<int>((std::max)(1.0f, (std::min)(float(kHardPhysicsMaxSteps), value)));
}
inline float PhysicsFrameForceSeconds(float delta, float hz, float maxSteps)
{
    if(!std::isfinite(delta) || delta <= 0) return 0;
    const double budget = (std::min)(kMaximumPhysicsFrameSeconds,
        double(ResolvePhysicsMaxSteps(maxSteps)) / ResolvePhysicsHz(hz));
    return static_cast<float>((std::min)(double(delta), budget));
}
struct PhysicsStepBatch
{
    int hz = kDefaultPhysicsHz;
    int steps = 0;
    double stepSeconds = 1.0 / kDefaultPhysicsHz;
    double debtSeconds = 0;
    double droppedSeconds = 0;
    bool reset = false;
};
class PhysicsStepScheduler
{
public:
    void Reset()
    {
        m_remainder = m_lastTime = 0;
        m_generation = 0;
        m_hasTime = false;
    }
    PhysicsStepBatch BeginFrame(double delta, float hz, float maxSteps,
        double gameTime, std::uint64_t generation)
    {
        PhysicsStepBatch batch;
        batch.hz = ResolvePhysicsHz(hz);
        batch.stepSeconds = 1.0 / batch.hz;
        const bool validDelta = std::isfinite(delta) && delta >= 0;
        const bool validTime = std::isfinite(gameTime);
        if(!validDelta || !validTime || generation != m_generation ||
            (m_hasTime && gameTime + 1e-6 < m_lastTime))
        {
            Reset();
            batch.reset = true;
        }
        m_generation = generation;
        m_hasTime = validTime;
        if(validTime) m_lastTime = gameTime;
        if(!validDelta || !validTime) return batch;

        batch.debtSeconds = m_remainder + delta;
        const double accepted = (std::min)(delta, kMaximumPhysicsFrameSeconds);
        batch.droppedSeconds = delta - accepted;
        const double pending = m_remainder + accepted;
        // Covers representation of the supported steps as engine floats. A
        // mathematically complete step must not be deferred by rounding alone.
        constexpr double epsilon = 1e-9;
        const int due = static_cast<int>(std::floor((pending + epsilon) / batch.stepSeconds));
        batch.steps = (std::min)(due, ResolvePhysicsMaxSteps(maxSteps));
        batch.droppedSeconds += (due - batch.steps) * batch.stepSeconds;
        m_remainder = (std::max)(0.0, pending - due * batch.stepSeconds);
        return batch;
    }
    double RemainderSeconds() const { return m_remainder; }
private:
    double m_remainder = 0, m_lastTime = 0;
    std::uint64_t m_generation = 0;
    bool m_hasTime = false;
};
}
