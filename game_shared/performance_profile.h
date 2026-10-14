#pragma once

// Optional, main-thread profiling. Each DLL owns its own collector. No clocks,
// allocation, logging or GPU calls are made by disabled measurement scopes.
#include <array>
#include <cstddef>
#include <cstdint>

namespace pxperf
{
enum class Metric : std::size_t
{
    FrameCpu, FrameInterval, GameDelta, ProfileReport,
    Physics, CollisionSync, PhysicsEvents, AnimationFollow, PhysicsDrives,
    PhysicsSubmit, PhysicsWait, PhysicsCapture, PhysicsPresentation,
    CollisionMesh, CollisionQuery, ImpactMesh, ServerBoneQuery,
    StudioBones, Ballistics, CurveBuild, BulletSweep, BulletImpact,
    WorldDraw, Subviews, Shadows, PostEffects, ViewModel,
    GpuFrame, GpuWorld, GpuSubviews, GpuShadows, GpuPostEffects, GpuViewModel,
    PhysicsSteps, PhysicsDebt, AwakeBodies, DynamicBodies,
    ActiveRagdolls, SleepingRagdolls, FrozenRagdolls,
    CollisionMeshBuilds, CollisionTriangles, ImpactMeshBuilds,
    BulletSweeps, BulletImpacts, ActiveShots, CurveSamples,
    BoneEvaluations, PoseCacheHits, MeshCacheHits, MeshCacheMisses,
    ShadowPasses, MirrorPasses, PortalPasses, ScreenPasses,
    GpuSkippedFrames, GpuDroppedScopes, GpuInvalidFrames,
    ServerPoseCacheHits, ServerPoseCacheMisses,
    CollisionPoseCacheHits, CollisionPoseCacheMisses,
    ImpactPoseCacheHits, ImpactPoseCacheMisses,
    RagdollPoseCaptures, SleepingCaptureReuses, RagdollBoundsQueries,
    RagdollPresentationUpdates, SleepingPresentationReuses, SleepingBoneReuses,
    PhysicsDroppedTime, PhysicsRemainder, PhysicsHz, PhysicsCatchupClamps, PhysicsClockResets,
    MonitorCacheHits, ReflectionCacheHits, ShadowCacheHits, ShadowBudgetSkips,
    CurveBuilds20ms, CurveBuilds10ms, CurveAccuracyFallbacks,
    Count
};
constexpr std::size_t kMetricCount = static_cast<std::size_t>(Metric::Count);
constexpr std::size_t kSampleCapacity = 2048;
constexpr std::size_t kGpuFirst = static_cast<std::size_t>(Metric::GpuFrame);
constexpr std::size_t kGpuCount = 6;

struct Statistics
{
    std::size_t samples = 0;
    double mean = 0, p50 = 0, p95 = 0, p99 = 0, peak = 0;
    std::uint64_t calls = 0;
};
using PrintLine = void (*)(const char *line);
double ClockSeconds();
const char *MetricName(Metric metric);
const char *MetricUnit(Metric metric);
bool IsGpuMetric(Metric metric);

class Collector
{
public:
    void BeginFrame(float mode, float interval, double gameTime, double gameDelta);
    void EndFrame();
    void Reset();
    bool Enabled() const { return m_mode != 0 && m_open; }
    std::uint64_t FrameGeneration() const { return m_generation; }
    std::uint64_t Session() const { return m_session; }
    void Add(Metric metric, double value, std::uint32_t calls = 1);
    void Set(Metric metric, double value);
    bool EnterScope(Metric metric);
    bool LeaveScope(Metric metric);
    // Completed asynchronous samples are per submitted GPU frame, not per
    // current CPU frame. Reject samples from a previous map/capture session.
    void AddGpuFrame(std::uint64_t session, const std::array<double, kGpuCount> &milliseconds,
        const std::array<std::uint32_t, kGpuCount> &calls, bool partial);
    Statistics Stats(Metric metric) const;
    bool ReportDue() const;
    void Report(const char *side, PrintLine print);
private:
    struct Series
    {
        std::array<double, kSampleCapacity> values{};
        std::array<std::uint32_t, kSampleCapacity> calls{};
        std::size_t size = 0, next = 0;
        double sum = 0;
        std::uint64_t callSum = 0;
        void Add(double value, std::uint32_t count);
        void Clear();
    };
    std::array<Series, kMetricCount> m_series{};
    std::array<double, kMetricCount> m_frame{};
    std::array<std::uint32_t, kMetricCount> m_calls{};
    std::array<std::uint32_t, kMetricCount> m_depth{};
    int m_mode = 0;
    bool m_open = false, m_hasPreviousStart = false, m_hasGameTime = false;
    double m_frameStart = 0, m_previousStart = 0, m_gameTime = 0;
    double m_windowStart = 0, m_interval = 2;
    double m_pendingReport = -1;
    std::uint64_t m_generation = 0, m_session = 1, m_report = 0;
};

Collector &Get();

class Scope
{
public:
    explicit Scope(Metric metric) : m_metric(metric), m_active(Get().Enabled())
    {
        if(m_active)
        {
            m_generation = Get().FrameGeneration();
            if(Get().EnterScope(metric)) m_start = ClockSeconds();
        }
    }
    ~Scope()
    {
        if(m_active && Get().Enabled() && m_generation == Get().FrameGeneration())
            Get().Add(m_metric, Get().LeaveScope(m_metric) ? (ClockSeconds() - m_start) * 1000.0 : 0);
    }
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
private:
    Metric m_metric;
    bool m_active;
    double m_start = 0;
    std::uint64_t m_generation = 0;
};
}
