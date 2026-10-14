#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
#ifdef BALLISTIC_WINDOWS_MACROS
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#include "ballistic_curve_policy.h"
#include "ballistic_path_cache.h"
extern "C" const char *__asan_default_options(){return "detect_leaks=0:halt_on_error=1";}

using namespace modular;
static BallisticVec3 add(BallisticVec3 a,BallisticVec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
static BallisticVec3 scale(BallisticVec3 a,double k){return {a.x*k,a.y*k,a.z*k};}
static double length(BallisticVec3 a){return std::hypot(std::hypot(a.x,a.y),a.z);}
static double distance(BallisticVec3 a,BallisticVec3 b){return length(add(a,scale(b,-1)));}
// Independent RK4 reference for dx/dt=v, dv/dt=g-k*|v|*v, ds/dt=|v|.
// This does not call the production integrator or interpolate its samples.
static BallisticState derivative(const BallisticState& s,const BallisticParameters& p){
    BallisticState d;d.position=s.velocity;
    const double k=.5*p.airDensity*p.dragCoefficient*3.141592653589793*p.diameterM*p.diameterM*.25/p.massKg;
    d.velocity=scale(s.velocity,-k*length(s.velocity));d.velocity.z-=p.gravity;
    d.seconds=1;d.meters=length(s.velocity);return d;
}
static BallisticState offset(BallisticState s,const BallisticState& d,double t){
    s.position=add(s.position,scale(d.position,t));s.velocity=add(s.velocity,scale(d.velocity,t));
    s.seconds+=d.seconds*t;s.meters+=d.meters*t;return s;
}
static BallisticState reference(BallisticState s,const BallisticParameters& p,double end,double dt=.0002){
    while(s.seconds<end-1e-12){
        const double h=(std::min)(dt,end-s.seconds);
        const auto a=derivative(s,p),b=derivative(offset(s,a,h*.5),p);
        const auto c=derivative(offset(s,b,h*.5),p),d=derivative(offset(s,c,h),p);
        s=offset(offset(offset(offset(s,a,h/6),b,h/3),c,h/3),d,h/6);
    }
    return s;
}
static void equal(const BallisticState& a,const BallisticState& b){
    assert(a.position.x==b.position.x && a.position.y==b.position.y && a.position.z==b.position.z);
    assert(a.velocity.x==b.velocity.x && a.velocity.y==b.velocity.y && a.velocity.z==b.velocity.z);
    assert(a.seconds==b.seconds && a.meters==b.meters);
}

static void policies(){
    BallisticState s;s.velocity={360,0,0};BallisticParameters p;
    assert(ChooseBallisticCurvePolicy(s,p,50).step==.02);
    assert(ChooseBallisticCurvePolicy(s,p,100).step==.01);
    for(double hz:{0.,-1.,1.,74.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})
        assert(ChooseBallisticCurvePolicy(s,p,hz).step==.02);
    for(double hz:{75.,100.,10000.})assert(ChooseBallisticCurvePolicy(s,p,hz).step==.01);
    p.massKg=.001;p.diameterM=.01;p.dragCoefficient=2;s.velocity={2000,0,0};
    const auto fine=ChooseBallisticCurvePolicy(s,p,50);
    assert(fine.step==.01 && fine.accuracyFallback && fine.SampleLimit()==1002);
    std::vector<BallisticState> guarded,legacy;
    assert(PrecomputeBallisticCurve(guarded,s,p,10000,fine.step,fine.SampleLimit()));
    assert(PrecomputeBallisticCurve(legacy,s,p,10000,.01,1002));
    assert(guarded.size()==legacy.size());
    for(size_t i=0;i<legacy.size();++i)equal(guarded[i],legacy[i]);
    for(double t:{.003,.017,.101,.733})equal(SampleBallisticCurve(guarded,t,fine.step),SampleBallisticCurve(legacy,t,.01));
    // Registry-permitted extreme dimensions/mass stay bounded and finite.
    p.massKg=.000001;p.diameterM=10;p.dragCoefficient=20;s.velocity={10000,0,0};
    assert(ChooseBallisticCurvePolicy(s,p,50).accuracyFallback);
    p.massKg=0;assert(ChooseBallisticCurvePolicy(s,p,50).accuracyFallback);
    std::cout<<"PASS policies: 50/100 Hz, invalid settings, conservative lifetime speed bound, exact retained high-drag path\n";
}

static void vacuumAndLimits(){
    BallisticParameters p;p.airDensity=0;BallisticState s;
    s.position={3,-2,5};s.velocity={360,20,40};s.seconds=.123;s.meters=2;
    std::vector<BallisticState> curve;
    assert(PrecomputeBallisticCurve(curve,s,p,10000,.02,502,true));
    assert(curve.size()<=502 && std::abs(curve.back().seconds-10)<1e-10);
    for(int i=1;i<1000;++i){
        const double t=i*.009731;
        const auto sampled=SampleBallisticCurve(curve,t,.02,true);
        const BallisticVec3 expected={3+360*t,-2+20*t,5+40*t-.5*p.gravity*t*t};
        assert(distance(sampled.position,expected)<1e-8);
        assert(std::abs(sampled.velocity.z-(40-p.gravity*t))<1e-8);
        assert(sampled.meters>=s.meters && sampled.meters<=curve.back().meters);
    }
    for(size_t i=0;i<curve.size();++i){
        auto sampled=SampleBallisticCurve(curve,curve[i].seconds-s.seconds,.02,true);
        assert(distance(sampled.position,curve[i].position)<1e-8);
    }
    equal(SampleBallisticCurve(curve,-1,.02,true),curve.front());
    equal(SampleBallisticCurve(curve,100,.02,true),curve.back());
    // Very short ranges, a partial final interval, nonzero initial age/range.
    for(double range:{2.001,2.1,5.0,9.3,73.7}){
        assert(PrecomputeBallisticCurve(curve,s,p,range,.02,502,true));
        assert(curve.back().meters==range && curve.back().seconds<10);
        for(int i=0;i<=100;++i){
            const auto q=SampleBallisticCurve(curve,(curve.back().seconds-s.seconds)*i/100,.02,true);
            assert(q.meters>=s.meters && q.meters<=range && std::isfinite(q.position.x));
        }
        equal(SampleBallisticCurve(curve,100,.02,true),curve.back());
    }
    s.seconds=9.998;assert(PrecomputeBallisticCurve(curve,s,p,10000,.02,502,true));
    assert(std::abs(curve.back().seconds-10)<1e-10 && curve.size()==2);
    assert(!PrecomputeBallisticCurve(curve,s,p,10000,.021,502));
    // Exact drag displacement composes to the analytic gravity-free solution.
    p.gravity=0;p.airDensity=1.225;s=BallisticState{};s.velocity={900,0,0};
    const double k=.5*p.airDensity*p.dragCoefficient*3.141592653589793*p.diameterM*p.diameterM*.25/p.massKg;
    auto integrated=s;
    for(int i=0;i<50;++i)integrated=IntegrateBallistic(integrated,p,.02,true);
    assert(std::abs(integrated.position.x-std::log1p(k*900)/k)<1e-9);
    assert(std::abs(integrated.velocity.x-900/(1+k*900))<1e-9);
    std::cout<<"PASS interpolation: 999 analytic vacuum samples, exact endpoints, range/lifetime clipping and invalid intervals\n";
}

static void accuracy(){
    const struct {const char* name;double mass,diameter,cd,speed;} rounds[]={
        {"9mm",.008,.009,.3,360},{"rifle",.004,.00556,.3,900},
        {"sniper",.016,.0127,.3,850},{"pellet",.0005,.0045,.4,250},
        {"slow",.008,.009,.3,20},{"heavy",.2,.04,.6,50}};
    const double root=std::sqrt(.5);
    const BallisticVec3 directions[]={{1,0,0},{root,0,root},{0,0,-1}};
    double peakPosition=0,peakEnergy=0,peakReference=0,peakLegacyPosition=0,peakCoarsePosition=0,peakCoarseEnergy=0;
    int scenarios=0,coarse=0,fine=0,samples=0;
    for(const auto& round:rounds)for(auto dir:directions)for(double gravity:{0.,9.80665,98.0665})for(double density:{0.,1.225,3.}){
        BallisticParameters p;p.massKg=round.mass;p.diameterM=round.diameter;
        p.dragCoefficient=round.cd;p.gravity=gravity;p.airDensity=density;
        BallisticState initial;initial.velocity=scale(dir,round.speed);
        const auto policy=ChooseBallisticCurvePolicy(initial,p,50);
        std::vector<BallisticState> curve,legacy;
        assert(PrecomputeBallisticCurve(curve,initial,p,100000,policy.step,policy.SampleLimit(),policy.step==.02));
        assert(PrecomputeBallisticCurve(legacy,initial,p,100000,.01,1002));
        ++scenarios;if(policy.step==.02)++coarse;else ++fine;
        auto ref=initial;
        double roundPeak=0;
        for(double t:{.001,.0033,.0067,.0099,.015,.019,.021,.035,.099,.101,.3333,.7333,1.,1.337,2.779,4.993,7.777,9.973}){
            if(t>=curve.back().seconds || t>=legacy.back().seconds)continue;
            ref=reference(ref,p,t);
            const auto actual=SampleBallisticCurve(curve,t,policy.step,policy.step==.02);
            const auto old=SampleBallisticCurve(legacy,t,.01);
            const double positionError=distance(actual.position,ref.position);
            const double energyError=std::abs(BallisticEnergy(actual,p.massKg)/BallisticEnergy(ref,p.massKg)-1);
            roundPeak=(std::max)(roundPeak,positionError);
            peakPosition=(std::max)(peakPosition,positionError);
            peakLegacyPosition=(std::max)(peakLegacyPosition,distance(old.position,ref.position));
            peakEnergy=(std::max)(peakEnergy,energyError);
            if(policy.step==.02){
                peakCoarsePosition=(std::max)(peakCoarsePosition,positionError);
                peakCoarseEnergy=(std::max)(peakCoarseEnergy,energyError);
            }
            if(policy.step==.01)equal(actual,old);
            if(positionError>=.05 || energyError>=.005)
                std::cerr<<"accuracy limit: "<<round.name<<" direction "<<dir.x<<","<<dir.z
                    <<" gravity "<<gravity<<" density "<<density<<" dt "<<policy.step
                    <<" t "<<t<<" position "<<positionError<<" energy "<<energyError<<"\n";
            assert(positionError<.05); // <= 5 cm over the tested 10-second envelope
            assert(energyError<.005); // <= 0.5% energy/damage coefficient error
            assert(actual.meters>=0 && actual.meters<=100000);
            ++samples;
        }
        // Independently halve reference dt to rule out reference-step error.
        if(gravity==9.80665 && density==1.225 && dir.x==1){
            const auto a=reference(initial,p,1,.0002),b=reference(initial,p,1,.0001);
            const double error=distance(a.position,b.position);assert(error<1e-6);
            peakReference=(std::max)(peakReference,error);
            std::cout<<"ACCURACY,"<<round.name<<",step_ms,"<<policy.step*1000<<",peak_position_m,"<<roundPeak<<"\n";
        }
    }
    assert(scenarios==162 && coarse>0 && fine>0 && samples>2000);
    std::cout<<"ACCURACY,total,"<<scenarios<<",coarse,"<<coarse<<",fine,"<<fine<<",samples,"<<samples
        <<",max_position_m,"<<peakPosition<<",max_energy_percent,"<<peakEnergy*100
        <<",coarse_max_position_m,"<<peakCoarsePosition
        <<",coarse_max_energy_percent,"<<peakCoarseEnergy*100
        <<",legacy_max_position_m,"<<peakLegacyPosition<<",reference_convergence_m,"<<peakReference<<"\n";
    std::cout<<"PASS accuracy: independent RK4 comparison, six rounds/three directions/gravity x0/1/10/density 0/1.225/3\n";
}

static void verticalTurningPointRegression(){
    // At a symmetric apex position returns to the starting point, but the
    // projectile traversed a nonzero physical distance. Formerly this made
    // PrecomputeBallisticCurve reject the entire shot as a zero-length step.
    BallisticParameters p;
    p.massKg = .25;
    p.gravity = 98.0665;
    p.airDensity = 0;
    const double dt = .02;
    const double v = p.gravity * dt * .5;
    BallisticState start;
    start.velocity = {0, 0, v};
    const double analyticDistance = p.gravity * dt * dt * .25;
    for (bool exact : {false, true}) {
        const auto next = IntegrateBallistic(start, p, dt, exact);
        assert(std::abs(next.position.z - start.position.z) < 1e-13);
        assert(next.meters > start.meters);
        assert(std::abs(next.meters - analyticDistance) < 1e-12);
        assert(next.seconds == dt);
        std::vector<BallisticState> curve;
        assert(PrecomputeBallisticCurve(curve, start, p, 100, dt, 502, exact));
        assert(curve.size() >= 2);
        assert(curve[1].meters > curve.front().meters);
        for (size_t i = 1; i < curve.size(); ++i)
            assert(curve[i].meters > curve[i-1].meters);
    }
    // The range check must still terminate inside this zero-displacement
    // interval using the accumulated arc length, not a vanished step.
    std::vector<BallisticState> shortCurve;
    assert(PrecomputeBallisticCurve(shortCurve, start, p, analyticDistance * .5, dt, 502, true));
    assert(std::abs(shortCurve.back().meters - analyticDistance * .5) < 1e-12);
    assert(shortCurve.back().seconds > 0 && shortCurve.back().seconds < dt);
    std::cout << "PASS vertical turning point: positive arc length, valid curve, range clipping\n";
}

static void work(){
    BallisticState s;s.velocity={360,0,0};BallisticParameters p;p.gravity=0;p.airDensity=0;
    std::vector<BallisticState> coarse,fine;
    assert(PrecomputeBallisticCurve(coarse,s,p,10000,.02,502,true));
    assert(PrecomputeBallisticCurve(fine,s,p,10000,.01,1002));
    assert(coarse.size()==501 && fine.size()==1001);
    for(double range:{10.,50.,200.,1000.}){
        assert(PrecomputeBallisticCurve(coarse,s,p,range,.02,502,true));
        assert(PrecomputeBallisticCurve(fine,s,p,range,.01,1002));
        assert(coarse.size()<=fine.size() && coarse.size()<=fine.size()/2+2);
    }
    p=BallisticParameters{};
    for(double dt:{.01,.02}){
        const auto start=std::chrono::steady_clock::now();size_t count=0;
        for(int i=0;i<2000;++i){assert(PrecomputeBallisticCurve(coarse,s,p,10000,dt,int(10/dt)+2,dt==.02));count+=coarse.size();}
        const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"CURVE_BENCHMARK,step_ms,"<<dt*1000<<",builds,2000,samples,"<<count<<",elapsed_ms,"<<elapsed<<"\n";
    }
    std::cout<<"PASS work: 1000 to 500 integration calls per full-lifetime vacuum curve, bounded short-range curves\n";
}
int main(int argc,char**){if(argc==2){work();return 0;}policies();vacuumAndLimits();verticalTurningPointRegression();accuracy();work();}
