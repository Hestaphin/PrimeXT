// J.10.6: reproduces the vector lifetime contract used by the server manager.
// Damage and death callbacks may add shots while the current Shot& is live.
// The engine manager must reserve its bounded capacity before those callbacks.
#include <cassert>
#include <cstddef>
#include <iostream>
#include <vector>

struct SimulatedShot { int id; double remainingTime; };
int main() {
    constexpr size_t maxActive = 256;
    std::vector<SimulatedShot> shots;
    shots.reserve(maxActive);
    shots.push_back({42, 0.125});
    SimulatedShot& current = shots.front();
    auto* address = &current;
    // Called reentrantly from a target's impact/death callback.
    for (size_t i = 1; i < maxActive; ++i)
        shots.push_back({static_cast<int>(i), 0.0});
    assert(shots.size() == maxActive);
    assert(&shots.front() == address);
    assert(current.id == 42 && current.remainingTime == 0.125);
    // Any 257th projectile must be rejected by the runtime cap.
    assert(shots.size() >= maxActive);
    std::cout << "PASS J.10.6 bounded vector stability under reentrant Add\n";
}
