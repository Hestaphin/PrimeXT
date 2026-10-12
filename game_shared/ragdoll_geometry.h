#pragma once
// Portable bind-space geometry used by authoring and the PhysX frame adapter.
#include "physics_asset_format.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>
namespace raggeometry
{
using V = physicsasset::Vec3;
constexpr double pi = 3.14159265358979323846;
inline V Add(V a,V b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline V Sub(V a,V b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline V Mul(V a,double s) { return {float(a.x*s),float(a.y*s),float(a.z*s)}; }
inline double Dot(V a,V b) { return double(a.x)*b.x+double(a.y)*b.y+double(a.z)*b.z; }
inline V Cross(V a,V b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline double Len(V a) { return std::sqrt(Dot(a,a)); }
inline V Unit(V a,V fallback={1,0,0}) { const double l=Len(a);return l>1e-8?Mul(a,1/l):fallback; }
struct Pose
{
    V x{1,0,0}, y{0,1,0}, z{0,0,1}, p{};
    V Rotate(V v) const { return Add(Add(Mul(x,v.x),Mul(y,v.y)),Mul(z,v.z)); }
    V InverseRotate(V v) const { return {float(Dot(x,v)),float(Dot(y,v)),float(Dot(z,v))}; }
    V Point(V v) const { return Add(p,Rotate(v)); }
    V InversePoint(V v) const { return InverseRotate(Sub(v,p)); }
};
inline Pose Euler(V a,V p={})
{
    const double pitch=a.x*pi/180,yaw=a.y*pi/180,roll=a.z*pi/180;
    const double cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw),cr=std::cos(roll),sr=std::sin(roll);
    return {{float(cp*cy),float(cp*sy),float(-sp)},
        {float(sr*sp*cy-cr*sy),float(sr*sp*sy+cr*cy),float(sr*cp)},
        {float(cr*sp*cy+sr*sy),float(cr*sp*sy-sr*cy),float(cr*cp)},p};
}
inline Pose Studio(V radians,V p)
{ return Euler({float(radians.y*180/pi),float(radians.z*180/pi),float(radians.x*180/pi)},p); }
inline Pose Concat(const Pose &a,const Pose &b)
{ return {a.Rotate(b.x),a.Rotate(b.y),a.Rotate(b.z),a.Point(b.p)}; }
inline Pose Axis(V x,V hint={0,0,1})
{
    x=Unit(x);V y=Sub(hint,Mul(x,Dot(x,hint)));
    if(Len(y)<1e-6) { hint=std::fabs(x.y)<0.9?V{0,1,0}:V{1,0,0};y=Sub(hint,Mul(x,Dot(x,hint))); }
    y=Unit(y);return {x,y,Cross(x,y),{}};
}
inline V Angles(const Pose &r)
{
    const double c=std::hypot(r.x.x,r.x.y);
    if(c>1e-6) return {float(std::atan2(-r.x.z,c)*180/pi),float(std::atan2(r.x.y,r.x.x)*180/pi),float(std::atan2(r.y.z,r.z.z)*180/pi)};
    return {float(std::atan2(-r.x.z,c)*180/pi),float(std::atan2(-r.y.x,r.y.y)*180/pi),0};
}
inline double PointSegment(V p,V a,V b)
{
    const V d=Sub(b,a);const double dd=Dot(d,d),t=dd>1e-12?std::clamp(Dot(Sub(p,a),d)/dd,0.0,1.0):0;
    return Len(Sub(p,Add(a,Mul(d,t))));
}
inline double SegmentDistance(V a,V b,V c,V d)
{
    // Closest points with both finite segment parameters clamped.
    const V u=Sub(b,a),v=Sub(d,c),w=Sub(a,c);
    const double A=Dot(u,u),B=Dot(u,v),C=Dot(v,v),D=Dot(u,w),E=Dot(v,w);
    if(A<1e-12) return PointSegment(a,c,d);
    if(C<1e-12) return PointSegment(c,a,b);
    const double denom=A*C-B*B;
    double s=denom>1e-12?std::clamp((B*E-C*D)/denom,0.0,1.0):0;
    double t=(B*s+E)/C;
    if(t<0) {t=0;s=std::clamp(-D/A,0.0,1.0);}
    else if(t>1) {t=1;s=std::clamp((B-D)/A,0.0,1.0);}
    return Len(Sub(Add(a,Mul(u,s)),Add(c,Mul(v,t))));
}
inline double PointBox(V p,V half)
{
    return Len({float(std::max(std::fabs(p.x)-half.x,0.0f)),float(std::max(std::fabs(p.y)-half.y,0.0f)),float(std::max(std::fabs(p.z)-half.z,0.0f))});
}
inline double SegmentBox(V a,V b,V h)
{
    const V d=Sub(b,a);const double av[]={a.x,a.y,a.z},dv[]={d.x,d.y,d.z},hv[]={h.x,h.y,h.z};
    std::vector<double> breaks{0,1};
    for(int i=0;i<3;++i) if(std::fabs(dv[i])>1e-12) for(int sign:{-1,1})
    { const double t=(sign*hv[i]-av[i])/dv[i];if(t>0&&t<1) breaks.push_back(t); }
    std::sort(breaks.begin(),breaks.end());double best=std::numeric_limits<double>::infinity();
    for(size_t j=0;j+1<breaks.size();++j)
    {
        const double lo=breaks[j],hi=breaks[j+1],mid=(lo+hi)*0.5;double aa=0,bb=0;
        for(int i=0;i<3;++i)
        {
            const double m=av[i]+dv[i]*mid;
            if(std::fabs(m)>hv[i]) { const double q=av[i]-(m>0?hv[i]:-hv[i]);aa+=dv[i]*dv[i];bb+=dv[i]*q; }
        }
        for(double t:{lo,hi,aa>1e-12?std::clamp(-bb/aa,lo,hi):mid}) best=std::min(best,PointBox(Add(a,Mul(d,t)),h));
    }
    return best;
}
inline double Overlap(const physicsasset::BodyDef &a,const Pose &boneA,const physicsasset::BodyDef &b,const Pose &boneB)
{
    using S=physicsasset::ShapeType;
    const Pose pa=Concat(boneA,Euler(a.angles,a.offset)),pb=Concat(boneB,Euler(b.angles,b.offset));
    const bool boxA=a.shape.type==S::Box,boxB=b.shape.type==S::Box;
    if(boxA&&boxB)
    {
        const V axesA[]={pa.x,pa.y,pa.z},axesB[]={pb.x,pb.y,pb.z};
        const double ha[]={a.shape.halfExtents.x,a.shape.halfExtents.y,a.shape.halfExtents.z},hb[]={b.shape.halfExtents.x,b.shape.halfExtents.y,b.shape.halfExtents.z};
        std::vector<V> axes{pa.x,pa.y,pa.z,pb.x,pb.y,pb.z};
        for(V x:axesA) for(V y:axesB) if(Len(Cross(x,y))>1e-6) axes.push_back(Unit(Cross(x,y)));
        double depth=std::numeric_limits<double>::infinity();
        for(V axis:axes)
        {
            double extent=0;for(int i=0;i<3;++i) extent+=ha[i]*std::fabs(Dot(axis,axesA[i]))+hb[i]*std::fabs(Dot(axis,axesB[i]));
            const double gap=extent-std::fabs(Dot(axis,Sub(pb.p,pa.p)));if(gap<=0) return 0;depth=std::min(depth,gap);
        }
        return depth;
    }
    const double la=a.shape.type==S::Capsule?a.shape.halfHeight:0,lb=b.shape.type==S::Capsule?b.shape.halfHeight:0;
    const V a0=pa.Point({float(-la),0,0}),a1=pa.Point({float(la),0,0}),b0=pb.Point({float(-lb),0,0}),b1=pb.Point({float(lb),0,0});
    if(boxA) return std::max(0.0,double(b.shape.radius)-SegmentBox(pa.InversePoint(b0),pa.InversePoint(b1),a.shape.halfExtents));
    if(boxB) return std::max(0.0,double(a.shape.radius)-SegmentBox(pb.InversePoint(a0),pb.InversePoint(a1),b.shape.halfExtents));
    return std::max(0.0,double(a.shape.radius)+b.shape.radius-SegmentDistance(a0,a1,b0,b1));
}
}
