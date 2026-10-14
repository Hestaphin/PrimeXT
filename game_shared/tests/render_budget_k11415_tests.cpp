#include <algorithm>
#include <cmath>
#include <cassert>
#include <cfloat>
#include <iostream>
#ifdef RENDER_BUDGET_WINDOWS_MACROS
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#include "render_budget.h"
extern "C" const char *__asan_default_options(){return "detect_leaks=0:halt_on_error=1";}
int main()
{
    using namespace renderbudget;
    for(float hz:{15.0f,30.0f})for(int fps:{30,60,144,240})for(double phase:{0.0,.37,.81})
    {
        double rendered=10;int calls=1;
        for(int frame=1;frame<=fps*2;++frame)
        {
            const double time=double(float(10.0+double(frame)/fps));
            if(RefreshDue(time,rendered,hz,phase)){++calls;rendered=time;}
        }
        assert(calls>=int(hz*2) && calls<=int(hz*2)+1);
        assert(!RefreshDue(rendered,rendered,hz,phase));
        assert(RefreshDue(rendered-1,rendered,hz,phase));
        assert(RefreshDue(rendered+10,rendered,hz,phase));
    }
    for(int limit:{0,4,8,16})
    {
        PassBudget budget;int draws=0;
        for(int cost:{4,6,1,6,1}) if(budget.Take(cost,limit))draws+=cost;
        assert(draws==budget.Used() && draws<=limit);
        budget.Reset();assert(budget.Used()==0);
    }
    PassBudget budget;assert(budget.Take(6,-1));assert(!budget.Take(-1,8));
    assert(MonitorHz(FLT_MAX)==30 && MonitorHz(1)==15 && MonitorHz(-1)==0 && MonitorHz(NAN)==30);
    assert(PassLimit(FLT_MAX)==512 && PassLimit(-1)==-1 && PassLimit(NAN)==16);
    assert(MonitorSize(FLT_MAX)==1024 && MonitorSize(-1)==128 && ReflectionScale(NAN)==.5f);
    auto size=ScaleSize(1920,1080,.5f,1920);assert(size.width==960 && size.height==540);
    size=ScaleSize(320,1024,1,512);assert(size.width==160 && size.height==512);
    size=ScaleSize(3840,2160,1,3840);assert(size.width==3840 && size.height==2160);
    assert(ScaleSize(0,1080,.5f,1024).width==0 && ScaleSize(1920,1080,.5f,0).width==0);
    std::cout<<"PASS render policies: 24 refresh schedules with float clocks and staggered phases, no backlog bursts, whole-group pass limits, bounded invalid controls and aspect-preserving resolutions\n";
}
