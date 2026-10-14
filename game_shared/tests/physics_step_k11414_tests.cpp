#include <algorithm>
#include <cassert>
#include <cmath>
#include <cfloat>
#include <iostream>
#include <limits>
#ifdef SCHEDULER_WINDOWS_MACROS
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#include "physics_step_scheduler.h"
#include "physics_drive_timestep.h"
extern "C" const char *__asan_default_options() { return "detect_leaks=0:halt_on_error=1"; }

static void near(double a, double b, double epsilon = 1e-7) { assert(std::fabs(a-b) < epsilon); }
int main()
{
    using namespace physicsasset;
    for(int hz : {30,60,100}) for(int fps : {30,60,72,144,240,1000})
    {
        PhysicsStepScheduler clock;
        double supplied=0,dropped=0,simulated=0;
        for(int frame=1;frame<=fps*3;++frame)
        {
            const float dt=1.0f/fps;supplied+=dt;
            const auto batch=clock.BeginFrame(dt,float(hz),4,double(frame)/fps,1);
            assert(batch.hz==hz && batch.steps>=0 && batch.steps<=4);
            assert(batch.droppedSeconds==0);
            near(batch.stepSeconds,1.0/hz);
            simulated+=batch.steps*batch.stepSeconds;dropped+=batch.droppedSeconds;
            assert(clock.RemainderSeconds()>=0 && clock.RemainderSeconds()<batch.stepSeconds);
        }
        near(simulated+dropped+clock.RemainderSeconds(),supplied,1e-6);
        assert(std::fabs(simulated-3)<1.0/hz+1e-5);
    }
    for(int hz : {30,60,100})
    {
        PhysicsStepScheduler clock;
        const auto batch=clock.BeginFrame(2,float(hz),4,2,1);
        assert(batch.steps==4 && batch.droppedSeconds>1.8);
        near(batch.steps*batch.stepSeconds+batch.droppedSeconds+clock.RemainderSeconds(),2);
        const auto next=clock.BeginFrame(.001,float(hz),4,2.001,1);
        assert(next.steps<=1 && next.droppedSeconds==0); // no whole-step debt returns
    }
    PhysicsStepScheduler clock;
    assert(clock.BeginFrame(double(float(.01)),100,4,0,1).steps==1);
    clock.Reset();assert(clock.RemainderSeconds()==0);
    auto batch=clock.BeginFrame(.03,30,4,1,1);assert(batch.steps==0);
    batch=clock.BeginFrame(0,60,4,1,1);assert(batch.steps==1); // preserves actual owed seconds
    near(clock.RemainderSeconds(),.03-1.0/60);
    batch=clock.BeginFrame(.001,60,4,.5,1);assert(batch.reset && batch.steps==0);
    near(clock.RemainderSeconds(),.001);
    batch=clock.BeginFrame(.001,60,4,.6,2);assert(batch.reset && batch.steps==0);
    near(clock.RemainderSeconds(),.001); // restore/map generation discards old fraction
    batch=clock.BeginFrame(0,60,4,.7,2);assert(batch.steps==0 && !batch.reset);
    for(double invalid : {-1.0, double(NAN), double(INFINITY)})
    {
        batch=clock.BeginFrame(invalid,60,4,1,2);
        assert(batch.reset && batch.steps==0 && clock.RemainderSeconds()==0);
    }
    assert(clock.BeginFrame(.1,60,4,NAN,2).steps==0);
    batch=clock.BeginFrame(FLT_MAX,60,FLT_MAX,2,2);
    assert(batch.steps==kHardPhysicsMaxSteps && std::isfinite(batch.droppedSeconds));
    assert(ResolvePhysicsMaxSteps(-FLT_MAX)==1 && ResolvePhysicsMaxSteps(NAN)==4);
    assert(ResolvePhysicsMaxSteps(3.9f)==3 && ResolvePhysicsHz(NAN)==60 && ResolvePhysicsHz(45)==60);
    clock.Reset();batch=clock.BeginFrame(.25,60,1,1,1);assert(batch.steps==1);
    // Independent semi-implicit integration of the returned PD force. The old
    // 80/s damping reverses velocity at 30 Hz; the longer-step drive converges
    // across body masses/inertia without changing authored gains or force units.
    for(int hz : {30,60}) for(float inverseMass : {.05f,1.0f,10.0f,100.0f})
    {
        const float dt=1.0f/hz;
        const auto gains=ResolveStepDriveGains(2000,80,dt,inverseMass);
        double position=10,velocity=0;
        for(int step=0;step<hz*10;++step)
        {
            velocity+=(-position*gains.spring-velocity*gains.damping)*inverseMass*dt;
            position+=velocity*dt;
            assert(std::isfinite(position) && std::isfinite(velocity));
            assert(std::fabs(position)<=10.001);
        }
        assert(std::fabs(position)<.001);
    }
    const auto legacy=ResolveStepDriveGains(2000,80,.01f);near(legacy.spring,2000);near(legacy.damping,80);
    assert(ResolveStepDriveGains(2000,80,NAN).spring==0);
    std::cout<<"PASS scheduler: 18 render-rate/rate schedules, time accounting, bounded stalls, mode changes, pause/invalid time, restore/rewind, cvar and float boundaries; 8 PD convergence cases and legacy gains\n";
}
