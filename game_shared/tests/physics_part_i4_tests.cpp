#include "../physics_part_motion.h"
#include <cassert>
#include <cmath>
#include <limits>
using namespace physicsasset;
static bool near(float a,float b,float tol=0.001f){ return std::fabs(a-b)<tol; }
int main()
{
    MotionPose a{},b{};
    b.position.x=4;
    auto v=EstimateMotion(a,b,0.1f);
    assert(v.valid && near(v.linear.x,40) && near(v.angular.z,0));
    // Clock rollback/stale data must not create a velocity impulse.
    assert(!EstimateMotion(a,b,0).valid);
    assert(!EstimateMotion(a,b,-1).valid);
    assert(!EstimateMotion(a,b,0.5f).valid);
    // Z-axis +90 degrees in 100 ms.
    b.rotation={0,0,0.707106781f,0.707106781f};
    v=EstimateMotion(a,b,0.1f);
    assert(v.valid && near(v.angular.z,15.70796f));
    b.rotation={0,0,-0.707106781f,-0.707106781f};
    v=EstimateMotion(a,b,0.1f);
    assert(v.valid && near(v.angular.z,15.70796f));
    b.position.x=20000;
    v=EstimateMotion(a,b,0.1f);
    assert(v.valid && near(v.linear.x,4000));
    b.position.x=std::numeric_limits<float>::quiet_NaN();
    assert(!EstimateMotion(a,b,0.1f).valid);
    return 0;
}
