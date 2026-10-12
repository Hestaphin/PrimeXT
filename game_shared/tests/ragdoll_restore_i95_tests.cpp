#include "ragdoll_restore_i95_policy.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main()
{
    using physicsasset::ScaleBonePivot;
    const auto scaled=ScaleBonePivot({20.f,3.f,-5.f},{2.f,2.f,2.f});
    assert(scaled.x==40.f && scaled.y==6.f && scaled.z==-10.f);
    const auto nonUniform=ScaleBonePivot({20.f,-3.f,5.f},{2.f,3.f,4.f});
    assert(nonUniform.x==40.f && nonUniform.y==-9.f && nonUniform.z==20.f);
    const char model[]="models/car.mdl";
    auto check=[&](const char *name,int count,int capacity,int valid,int full,int frozen) {
        return physicsasset::ValidRagdollSnapshotHeader(name,128,model,count,capacity,valid,full,frozen);
    };
    char name[128]="models/car.mdl";
    assert(check(name,8,64,1,1,1));
    assert(check(name,64,64,1,0,0));
    assert(!check(name,65,64,1,1,0));
    assert(!check(name,0,64,1,1,0));
    assert(!check(name,8,64,0,1,0));
    assert(!check(name,8,64,1,2,0));
    assert(!check(name,8,64,1,1,2));
    name[0]='X';assert(!check(name,8,64,1,1,0));
    for(char &c:name) c='x';
    assert(!check(name,8,64,1,1,0));
    std::cout<<"I.9.5 pivot and ragdoll restore header checks PASS\n";
}
