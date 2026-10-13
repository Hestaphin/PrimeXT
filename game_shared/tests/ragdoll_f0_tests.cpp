#include "ragdoll_diagnostics.h"
#include "studio_pose_diagnostics.h"
#include <array>
#include <cassert>
#include <iostream>
#include <limits>

using namespace ragdolldiag;
static void Near(double a, double b, double tolerance = 0.002) { assert(std::fabs(a-b) < tolerance); }
struct Bone { int parent; };

static void PoseCase(float scale)
{
    const std::array<Bone,4> bones{{{-1},{0},{1},{2}}};
    const std::array<bool,4> driven{{false,true,true,false}};
    std::array<matrix3x4,4> animated, finalBones, targets;
    const Vector turn(15,37,-12), zero(0,0,0);
    animated[0] = matrix3x4(Vector(0,0,0),zero,scale);
    animated[1] = matrix3x4(Vector(0,0,4*scale),zero,scale);
    animated[2] = matrix3x4(Vector(0,8*scale,4*scale),zero,scale);
    animated[3] = matrix3x4(Vector(0,12*scale,4*scale),zero,scale);
    finalBones = animated;
    finalBones[1] = matrix3x4(Vector(20,40,12),turn,scale);
    finalBones[2] = physicsasset::StudioRebuildPhysicalChild(finalBones[1],
        physicsasset::StudioAnimatedWorldRelative(animated[1],animated[2]),scale);
    finalBones[3] = physicsasset::StudioRebuildPhysicalChild(finalBones[2],
        physicsasset::StudioAnimatedWorldRelative(animated[2],animated[3]),scale);
    targets = finalBones;
    auto result = MeasureStudioPose(4,bones.data(),animated.data(),finalBones.data(),driven,targets.data(),1);
    assert(result.valid && result.bones == 4 && result.drivenCompared == 2);
    assert(result.ancestorsCompared == 1 && result.descendantsCompared == 1);
    assert(result.ancestorPositionDelta > 20); // pxmv-style unmoved Studio master
    assert(result.worstAncestorBone == 0);
    Near(result.drivenPositionDelta,0); Near(result.descendantPositionDelta,0);
    Near(result.descendantAngleDelta,0,0.06);
    // A correct reverse link removes the ancestor discrepancy without the
    // diagnostic code ever repairing or altering that link itself.
    finalBones[0] = physicsasset::StudioRebuildPhysicalChild(finalBones[1],
        physicsasset::StudioAnimatedWorldRelative(animated[1],animated[0]),scale);
    result = MeasureStudioPose(4,bones.data(),animated.data(),finalBones.data(),driven,targets.data(),1);
    Near(result.ancestorPositionDelta,0); Near(result.ancestorAngleDelta,0,0.06);
    Near(result.edgeLengthDelta,0);
    const auto before = finalBones;
    finalBones[3].SetOrigin(finalBones[3].GetOrigin()+Vector(100,0,0));
    result = MeasureStudioPose(4,bones.data(),animated.data(),finalBones.data(),driven,targets.data(),1);
    Near(result.descendantPositionDelta,100);
    assert(result.worstDescendantBone == 3 && result.edgeLengthDelta > 50);
    Near(Distance(StudioDiagnosticPose(finalBones[3]).position,StudioDiagnosticPose(before[3]).position),100);
    // Driven target discrepancy is distinct from the passive-child link.
    targets[2].SetOrigin(targets[2].GetOrigin()+Vector(0,7,0));
    result = MeasureStudioPose(4,bones.data(),animated.data(),finalBones.data(),driven,targets.data(),1);
    Near(result.drivenPositionDelta,7);
    assert(result.worstDrivenBone == 2);
    finalBones[2].SetOrigin(Vector(std::numeric_limits<float>::quiet_NaN(),0,0));
    result = MeasureStudioPose(4,bones.data(),animated.data(),finalBones.data(),driven,targets.data(),1);
    assert(result.invalidBones == 1 && result.invalidComparisons > 0);
}

int main()
{
    Near(Length({3,4,0}),5); Near(Distance({1,2,3},{1,2,8}),5);
    assert(Length({std::numeric_limits<double>::infinity(),0,0}) == -1);
    Near(RotationDeltaDegrees({0,0,0,1},{0,0,0,-2}),0);
    Near(RotationDeltaDegrees({0,0,0,1},{1,0,0,0}),180);
    assert(RotationDeltaDegrees({0,0,0,0},{0,0,0,1}) == -1);
    Near(IntervalExcess(0,-0.5,0.5),0); Near(IntervalExcess(0.7,-0.5,0.5),0.2);
    Near(IntervalExcess(-0.8,-0.5,0.5),0.3); assert(IntervalExcess(0,1,-1) == -1);
    ReferenceSpan reference;
    Near(reference.Value(),0); reference.Add({1,2,3}); reference.Add({1,4,67}); Near(reference.Value(),64);
    reference.Add({0,0,std::numeric_limits<double>::quiet_NaN()}); Near(reference.Value(),64);
    Cadence clock;
    assert(!clock.Advance(0,1) && !clock.Advance(-1,1));
    assert(clock.Advance(0.01,1));
    for(int i = 0; i < 99; ++i) assert(!clock.Advance(0.01,1));
    assert(clock.Advance(0.01,1)); Near(clock.Elapsed(),1.01);
    clock.Reset(); assert(clock.Advance(1.0/60.0,0.25));
    assert(clock.Advance(0.01,0)); assert(!clock.Advance(std::numeric_limits<double>::quiet_NaN(),0));
    Window window;
    Snapshot first; first.stepSeconds=0.01; first.maxAngularSpeed=25; first.rootLinearSpeed=8;
    first.maxAnchorSeparation=3; first.worstAnchorJoint=4; first.invalidSamples=2; first.bodies=5;
    first.inputFrameSeconds=0.1; first.accumulatorSeconds=0.04;
    window.Push(first);
    Snapshot settled; settled.stepSeconds=0.01; settled.bodies=5; settled.sleeping=5;
    settled.maxAxisExcessDegrees=7; settled.worstAxisJoint=1;
    window.Push(settled);
    PoseMetrics pose; pose.valid=true; pose.ancestorPositionDelta=12; pose.worstAncestorBone=0;
    window.PushPose(pose);
    PoseMetrics angular; angular.valid=true; angular.ancestorAngleDelta=75; angular.worstAncestorBone=3;
    window.PushPose(angular);
    auto aggregate=window.Take(); assert(window.Empty());
    assert(aggregate.windowSamples==2 && aggregate.sleeping==5 && aggregate.invalidSamples==2);
    Near(aggregate.windowSeconds,0.02); Near(aggregate.maxAngularSpeed,25); Near(aggregate.rootLinearSpeed,8);
    Near(aggregate.inputFrameSeconds,0.1); Near(aggregate.accumulatorSeconds,0.04);
    Near(aggregate.maxAnchorSeparation,3); assert(aggregate.worstAnchorJoint==4);
    Near(aggregate.maxAxisExcessDegrees,7); assert(aggregate.worstAxisJoint==1 && aggregate.pose.valid);
    Near(aggregate.pose.ancestorPositionDelta,12); Near(aggregate.pose.ancestorAngleDelta,75);
    assert(aggregate.pose.worstAncestorBone==3); // an angular-only peak must identify its bone
    window.Push(settled); Near(window.Take().maxAngularSpeed,0); // no old-window leak
    const std::string row=CsvRow(aggregate), header=CsvHeader();
    assert(std::count(row.begin(),row.end(),',')==std::count(header.begin(),header.end(),','));
    assert(row.find("nan")==std::string::npos && row.find("inf")==std::string::npos);
    PoseCase(1); PoseCase(0.5f); PoseCase(2.5f);
    std::array<Bone,3> cycle{{{1},{0},{1}}};
    std::array<matrix3x4,3> matrices; for(auto &m:matrices)m.Identity();
    const std::array<bool,3> mask{{false,false,true}};
    const auto cycleResult=MeasureStudioPose(3,cycle.data(),matrices.data(),matrices.data(),mask,matrices.data(),2);
    assert(cycleResult.invalidParents>=2);
    assert(!MeasureStudioPose(0,cycle.data(),matrices.data(),matrices.data(),mask,matrices.data(),-1).valid);
    std::cout << "PASS F0 measurement: sign-safe quaternions, invalid input, throttling/window peaks, scale/rotation links, unmoved ancestors, injected child spikes and cycle bounds\n";
}
