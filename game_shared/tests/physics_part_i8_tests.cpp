#include "physics_part_net.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
int main()
{
    static_assert(partnet::kWireBytes == 37, "wire format drift");
    assert(partnet::kActiveInterval > 0.0f && partnet::kActiveInterval <= 0.1f);
    assert(partnet::kSleepInterval > partnet::kActiveInterval);
    assert(partnet::Newer(3u,2u));
    assert(!partnet::Newer(3u,3u));
    assert(!partnet::Newer(2u,3u));
    assert(partnet::Newer(1u,0xffffffffu));
    assert(!partnet::Newer(0xffffffffu,1u));
    assert(partnet::Newer(1u,0u));
    assert(!partnet::Newer(0u,0u));
    assert(partnet::ValidQuat(0.f,0.f,0.f,1.f));
    assert(!partnet::ValidQuat(0.f,0.f,0.f,0.f));
    assert(!partnet::ValidQuat(0.f,0.f,0.f,std::numeric_limits<float>::infinity()));
    assert(partnet::ValidPosition(1.f,-2.f,3.f));
    assert(!partnet::ValidPosition(0.f,std::numeric_limits<float>::quiet_NaN(),0.f));
}
