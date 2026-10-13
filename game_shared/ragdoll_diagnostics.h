#pragma once

// J.10.6.10 / F0: measurement only. No sleeping, forces, projection, clamps,
// filtering or pose corrections are performed by this header.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

// Windows headers can define function-like min/max macros.
// Parenthesizing the C++ function names prevents macro expansion without
// altering or undefining macros used by other engine code.

namespace ragdolldiag
{
struct Vec3 { double x = 0, y = 0, z = 0; };
struct Quat { double x = 0, y = 0, z = 0, w = 1; };
struct Pose { Vec3 position; Quat rotation; };

inline bool Finite(const Vec3 &v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline double Length(const Vec3 &v)
{
    return Finite(v) ? std::hypot(std::hypot(v.x, v.y), v.z) : -1;
}

inline double Distance(const Vec3 &a, const Vec3 &b)
{
    return Length({a.x - b.x, a.y - b.y, a.z - b.z});
}

// q and -q are identical poses. Normalize the inputs, without altering them.
// -1 means unavailable/invalid; callers must count it, never treat it as zero.
inline double RotationDeltaDegrees(const Quat &a, const Quat &b)
{
    const double na = std::hypot(std::hypot(a.x, a.y), std::hypot(a.z, a.w));
    const double nb = std::hypot(std::hypot(b.x, b.y), std::hypot(b.z, b.w));
    if(!std::isfinite(na) || !std::isfinite(nb) || na < 1e-12 || nb < 1e-12) return -1;
    const double dot = (a.x/na)*(b.x/nb) + (a.y/na)*(b.y/nb) +
        (a.z/na)*(b.z/nb) + (a.w/na)*(b.w/nb);
    if(!std::isfinite(dot)) return -1;
    return 2 * std::acos(std::clamp(std::fabs(dot), 0.0, 1.0)) * (180.0 / 3.14159265358979323846);
}

inline double IntervalExcess(double angle, double lower, double upper)
{
    if(!std::isfinite(angle) || !std::isfinite(lower) || !std::isfinite(upper) || lower > upper) return -1;
    return (std::max)({0.0, lower - angle, angle - upper});
}

// Constant bind-pivot envelope, never the shrinking/rotating fallen AABB.
// This is a reference SPAN, not a measured mesh height.
struct ReferenceSpan
{
    Vec3 minimum, maximum;
    bool present = false;
    void Add(const Vec3 &v)
    {
        if(!Finite(v)) return;
        if(!present) { minimum = maximum = v; present = true; return; }
        minimum = {(std::min)(minimum.x,v.x), (std::min)(minimum.y,v.y), (std::min)(minimum.z,v.z)};
        maximum = {(std::max)(maximum.x,v.x), (std::max)(maximum.y,v.y), (std::max)(maximum.z,v.z)};
    }
    double Value() const
    {
        return present ? (std::max)({maximum.x-minimum.x,maximum.y-minimum.y,maximum.z-minimum.z}) : 0;
    }
};

struct PoseMetrics
{
    bool valid = false;
    unsigned bones = 0, invalidBones = 0, invalidComparisons = 0, invalidParents = 0;
    unsigned drivenCompared = 0, descendantsCompared = 0, ancestorsCompared = 0;
    double drivenPositionDelta = 0, drivenAngleDelta = 0;
    double descendantPositionDelta = 0, descendantAngleDelta = 0;
    double ancestorPositionDelta = 0, ancestorAngleDelta = 0;
    double edgeLengthDelta = 0;
    int worstDrivenBone = -1, worstDescendantBone = -1, worstAncestorBone = -1, worstEdgeBone = -1;
};

inline void ComparePose(PoseMetrics &m, const Pose &actual, const Pose &expected,
    int bone, double &positionPeak, double &anglePeak, int &worstBone)
{
    const double position = Distance(actual.position, expected.position);
    const double angle = RotationDeltaDegrees(actual.rotation, expected.rotation);
    if(position < 0 || !std::isfinite(position) || angle < 0 || !std::isfinite(angle))
    {
        ++m.invalidComparisons;
        return;
    }
    if(position > positionPeak || angle > anglePeak) worstBone = bone;
    positionPeak = (std::max)(positionPeak, position);
    anglePeak = (std::max)(anglePeak, angle);
}

struct Snapshot
{
    double simulationSeconds = 0, stepSeconds = 0, windowSeconds = 0;
    double inputFrameSeconds = -1, accumulatorSeconds = -1;
    unsigned windowSamples = 0;
    unsigned bodies = 0, active = 0, sleeping = 0, kinematic = 0, missingBodies = 0;
    unsigned joints = 0, brokenJoints = 0, missingJoints = 0, invalidSamples = 0;
    double rootLinearSpeed = -1, rootAngularSpeed = -1;
    double maxLinearSpeed = 0, maxAngularSpeed = 0, maxRelativeAngularSpeed = 0;
    double maxAnchorSeparation = 0, bindSpan = 0;
    // Conservative axis overshoot: NOT the coupled D6 swing-cone solver error.
    double maxAxisExcessDegrees = 0, samplingMilliseconds = 0;
    int worstAnchorJoint = -1, worstAxisJoint = -1;
    int sceneDiscretePairs = -1, scenePairsWithContacts = -1; // scene-wide, non-CCD
    PoseMetrics pose;
};

inline void MergePose(PoseMetrics &a, const PoseMetrics &b)
{
    if(!b.valid) return;
    if(!a.valid) { a = b; return; }
    a.bones = b.bones;
    a.drivenCompared = b.drivenCompared;
    a.descendantsCompared = b.descendantsCompared;
    a.ancestorsCompared = b.ancestorsCompared;
    a.invalidBones = (std::max)(a.invalidBones, b.invalidBones);
    a.invalidComparisons = (std::max)(a.invalidComparisons, b.invalidComparisons);
    a.invalidParents = (std::max)(a.invalidParents, b.invalidParents);
    auto peak = [](double &p, double value, int &i, int index)
    { if(value > p) { p = value; i = index; } };
    auto posePeak = [](double &position, double &angle, double nextPosition, double nextAngle, int &i, int index)
    {
        if(nextPosition > position || nextAngle > angle) i = index;
        position = (std::max)(position,nextPosition);
        angle = (std::max)(angle,nextAngle);
    };
    posePeak(a.drivenPositionDelta,a.drivenAngleDelta,b.drivenPositionDelta,b.drivenAngleDelta,a.worstDrivenBone,b.worstDrivenBone);
    posePeak(a.descendantPositionDelta,a.descendantAngleDelta,b.descendantPositionDelta,b.descendantAngleDelta,a.worstDescendantBone,b.worstDescendantBone);
    posePeak(a.ancestorPositionDelta,a.ancestorAngleDelta,b.ancestorPositionDelta,b.ancestorAngleDelta,a.worstAncestorBone,b.worstAncestorBone);
    peak(a.edgeLengthDelta,b.edgeLengthDelta,a.worstEdgeBone,b.worstEdgeBone);
}

// Peaks survive between throttled reports; counts describe the latest sample.
class Window
{
public:
    bool Empty() const { return m_value.windowSamples == 0; }
    void Reset() { m_value = Snapshot{}; }
    void Push(const Snapshot &sample)
    {
        Snapshot next = sample;
        next.windowSamples = m_value.windowSamples + 1;
        next.windowSeconds = m_value.windowSeconds + sample.stepSeconds;
        next.rootLinearSpeed = (std::max)(m_value.rootLinearSpeed,sample.rootLinearSpeed);
        next.rootAngularSpeed = (std::max)(m_value.rootAngularSpeed,sample.rootAngularSpeed);
        next.maxLinearSpeed = (std::max)(m_value.maxLinearSpeed,sample.maxLinearSpeed);
        next.maxAngularSpeed = (std::max)(m_value.maxAngularSpeed,sample.maxAngularSpeed);
        next.maxRelativeAngularSpeed = (std::max)(m_value.maxRelativeAngularSpeed,sample.maxRelativeAngularSpeed);
        next.inputFrameSeconds = (std::max)(m_value.inputFrameSeconds,sample.inputFrameSeconds);
        next.accumulatorSeconds = (std::max)(m_value.accumulatorSeconds,sample.accumulatorSeconds);
        next.samplingMilliseconds = (std::max)(m_value.samplingMilliseconds,sample.samplingMilliseconds);
        next.invalidSamples = (std::max)(m_value.invalidSamples,sample.invalidSamples);
        if(m_value.maxAnchorSeparation > next.maxAnchorSeparation)
        { next.maxAnchorSeparation = m_value.maxAnchorSeparation; next.worstAnchorJoint = m_value.worstAnchorJoint; }
        if(m_value.maxAxisExcessDegrees > next.maxAxisExcessDegrees)
        { next.maxAxisExcessDegrees = m_value.maxAxisExcessDegrees; next.worstAxisJoint = m_value.worstAxisJoint; }
        next.pose = m_value.pose;
        MergePose(next.pose,sample.pose);
        m_value = next;
    }
    void PushPose(const PoseMetrics &pose) { MergePose(m_value.pose,pose); }
    Snapshot Take() { const Snapshot result = m_value; Reset(); return result; }
private:
    Snapshot m_value;
};

class Cadence
{
public:
    void Reset() { m_elapsed = m_since = 0; m_first = true; }
    double Elapsed() const { return m_elapsed; }
    bool Advance(double step, double period)
    {
        if(!std::isfinite(step) || step <= 0 || !std::isfinite(period) || period < 0) return false;
        m_elapsed += step;
        m_since += step;
        // Runtime steps originate as float (0.01f); allow their rounding at a
        // report boundary without losing a whole extra reporting substep.
        if(m_first || period == 0 || m_since + 1e-7 * (std::max)(1.0,period) >= period)
        { m_first = false; m_since = 0; return true; }
        return false;
    }
private:
    double m_elapsed = 0, m_since = 0;
    bool m_first = true;
};

inline const char *CsvHeader()
{
    return "sim_s,step_s,window_s,samples,bodies,active,sleeping,kinematic,missing_bodies,joints,broken_joints,missing_joints,invalid_samples,root_linear_peak,root_angular_peak,linear_peak,angular_peak,relative_angular_peak,anchor_peak,bind_span,anchor_pct_bind_span,worst_anchor_joint,axis_excess_deg,worst_axis_joint,scene_discrete_pairs,scene_pairs_with_contacts,sampling_ms_peak,pose_valid,pose_bones,pose_invalid_bones,pose_invalid_comparisons,pose_invalid_parents,driven_compared,descendants_compared,ancestors_compared,driven_position_delta,driven_angle_delta,descendant_position_delta,descendant_angle_delta,ancestor_position_delta,ancestor_angle_delta,edge_length_delta,worst_driven_bone,worst_descendant_bone,worst_ancestor_bone,worst_edge_bone,frame_input_s_peak,backlog_s_peak";
}

inline std::string CsvRow(const Snapshot &s)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(8);
    const double percentage = s.bindSpan > 1e-6 ? 100 * s.maxAnchorSeparation / s.bindSpan : -1;
    out << s.simulationSeconds << ',' << s.stepSeconds << ',' << s.windowSeconds << ',' << s.windowSamples
        << ',' << s.bodies << ',' << s.active << ',' << s.sleeping << ',' << s.kinematic << ',' << s.missingBodies
        << ',' << s.joints << ',' << s.brokenJoints << ',' << s.missingJoints << ',' << s.invalidSamples
        << ',' << s.rootLinearSpeed << ',' << s.rootAngularSpeed << ',' << s.maxLinearSpeed << ',' << s.maxAngularSpeed
        << ',' << s.maxRelativeAngularSpeed << ',' << s.maxAnchorSeparation << ',' << s.bindSpan << ',' << percentage
        << ',' << s.worstAnchorJoint << ',' << s.maxAxisExcessDegrees << ',' << s.worstAxisJoint
        << ',' << s.sceneDiscretePairs << ',' << s.scenePairsWithContacts << ',' << s.samplingMilliseconds;
    const PoseMetrics &p = s.pose;
    out << ',' << (p.valid ? 1 : 0) << ',' << p.bones << ',' << p.invalidBones << ',' << p.invalidComparisons
        << ',' << p.invalidParents << ',' << p.drivenCompared << ',' << p.descendantsCompared << ',' << p.ancestorsCompared
        << ',' << p.drivenPositionDelta << ',' << p.drivenAngleDelta << ',' << p.descendantPositionDelta
        << ',' << p.descendantAngleDelta << ',' << p.ancestorPositionDelta << ',' << p.ancestorAngleDelta
        << ',' << p.edgeLengthDelta << ',' << p.worstDrivenBone << ',' << p.worstDescendantBone
        << ',' << p.worstAncestorBone << ',' << p.worstEdgeBone << ',' << s.inputFrameSeconds << ',' << s.accumulatorSeconds;
    return out.str();
}
}
