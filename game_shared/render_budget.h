#pragma once
#include <algorithm>
#include <cmath>

namespace renderbudget
{
inline float MonitorHz(float value)
{
    if(!std::isfinite(value)) return 30;
    return value <= 0 ? 0 : (std::max)(15.0f,(std::min)(30.0f,value));
}
inline int PassLimit(float value)
{
    if(!std::isfinite(value)) return 16;
    return value < 0 ? -1 : int((std::min)(512.0f,value));
}
inline float ReflectionScale(float value)
{
    return std::isfinite(value) ? (std::max)(.25f,(std::min)(1.0f,value)) : .5f;
}
inline int MonitorSize(float value)
{
    return std::isfinite(value) ? int((std::max)(128.0f,(std::min)(1024.0f,value))) : 512;
}
class PassBudget
{
public:
    void Reset() { m_used=0; }
    bool Take(int cost,int limit)
    {
        if(cost<0 || (limit>=0 && cost>limit-m_used)) return false;
        m_used+=cost;return true;
    }
    int Used() const { return m_used; }
private:
    int m_used=0;
};
inline bool RefreshDue(double now,double rendered,float hz,double phase=0)
{
    if(!std::isfinite(now) || now<rendered || hz<=0) return true;
    // Fixed time buckets avoid cadence drift at 60/144 FPS. Skipped buckets
    // cause one refresh, never a burst. Tolerance covers float engine clocks.
    const auto tick=[&](double time){return std::floor(time*hz-phase+1e-4);};
    return tick(now)>tick(rendered);
}
struct Size { int width,height; };
inline Size ScaleSize(int width,int height,float scale,int maximum)
{
    if(width<=0 || height<=0 || maximum<=0) return {0,0};
    const double factor=(std::min)(double(ReflectionScale(scale)),
        double(maximum)/(std::max)(width,height));
    return {(std::max)(1,int(std::floor(width*factor))),
        (std::max)(1,int(std::floor(height*factor)))};
}
}
