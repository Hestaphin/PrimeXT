#include "performance_profile.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>

namespace pxperf
{
namespace
{
constexpr const char *kNames[] = {
    "frame_cpu", "frame_interval", "game_delta", "profile_report",
    "physics", "collision_sync", "physics_events", "animation_follow", "physics_drives",
    "physics_submit", "physics_wait", "physics_capture", "physics_presentation",
    "collision_mesh", "collision_query", "impact_mesh", "server_bone_query",
    "studio_bones", "ballistics", "curve_build", "bullet_sweep", "bullet_impact",
    "world_draw", "subviews", "shadows", "post_effects", "viewmodel",
    "gpu_frame", "gpu_world", "gpu_subviews", "gpu_shadows", "gpu_post_effects", "gpu_viewmodel",
    "physics_steps", "physics_debt", "awake_bodies", "dynamic_bodies",
    "active_ragdolls", "sleeping_ragdolls", "frozen_ragdolls",
    "collision_mesh_builds", "collision_triangles", "impact_mesh_builds",
    "bullet_sweeps", "bullet_impacts", "active_shots", "curve_samples",
    "bone_evaluations", "pose_cache_hits", "mesh_cache_hits", "mesh_cache_misses",
    "shadow_passes", "mirror_passes", "portal_passes", "screen_passes",
    "gpu_skipped_frames", "gpu_dropped_scopes", "gpu_invalid_frames",
    "server_pose_cache_hits", "server_pose_cache_misses",
    "collision_pose_cache_hits", "collision_pose_cache_misses",
    "impact_pose_cache_hits", "impact_pose_cache_misses",
    "ragdoll_pose_captures", "sleeping_capture_reuses", "ragdoll_bounds_queries",
    "ragdoll_presentation_updates", "sleeping_presentation_reuses", "sleeping_bone_reuses",
    "physics_dropped_time", "physics_remainder", "physics_hz", "physics_catchup_clamps", "physics_clock_resets",
    "monitor_cache_hits", "reflection_cache_hits", "shadow_cache_hits", "shadow_budget_skips",
    "curve_builds_20ms", "curve_builds_10ms", "curve_accuracy_fallbacks"
};
static_assert(sizeof(kNames) / sizeof(kNames[0]) == kMetricCount, "Metric names must match enum");
bool Valid(Metric metric) { return static_cast<std::size_t>(metric) < kMetricCount; }
std::uint32_t SumCalls(std::uint32_t a, std::uint32_t b)
{
    const auto limit = (std::numeric_limits<std::uint32_t>::max)();
    return b > limit - a ? limit : a + b;
}
}

#ifndef PXPERF_TEST_CLOCK
double ClockSeconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
#endif
Collector &Get() { static Collector collector; return collector; }
const char *MetricName(Metric metric) { return Valid(metric) ? kNames[static_cast<std::size_t>(metric)] : "invalid"; }
bool IsGpuMetric(Metric metric)
{
    const auto index = static_cast<std::size_t>(metric);
    return index >= kGpuFirst && index < kGpuFirst + kGpuCount;
}
const char *MetricUnit(Metric metric)
{
    return static_cast<std::size_t>(metric) < static_cast<std::size_t>(Metric::PhysicsSteps) ||
        metric == Metric::PhysicsDebt || metric == Metric::PhysicsDroppedTime ||
        metric == Metric::PhysicsRemainder ? "ms" : metric == Metric::PhysicsHz ? "hz" : "count";
}
void Collector::Series::Add(double value, std::uint32_t count)
{
    if(size == kSampleCapacity) { sum -= values[next]; callSum -= calls[next]; }
    else ++size;
    values[next] = value; calls[next] = count;
    sum += value; callSum += count;
    next = (next + 1) % kSampleCapacity;
}
void Collector::Series::Clear() { size = next = 0; sum = 0; callSum = 0; }
void Collector::Reset()
{
    for(auto &series : m_series) series.Clear();
    m_frame.fill(0); m_calls.fill(0); m_depth.fill(0);
    m_mode = 0; m_open = m_hasPreviousStart = m_hasGameTime = false;
    m_windowStart = m_previousStart = m_frameStart = m_gameTime = 0;
    m_pendingReport = -1;
    ++m_session; ++m_generation;
}
void Collector::BeginFrame(float mode, float interval, double gameTime, double gameDelta)
{
    const int selected = !std::isfinite(mode) || mode <= 0 ? 0 : mode >= 2 ? 2 : 1;
    const bool rewound = std::isfinite(gameTime) && m_hasGameTime && gameTime + 1e-6 < m_gameTime;
    if(selected != m_mode || rewound) { Reset(); m_mode = selected; }
    if(!m_mode) return;
    m_interval = std::isfinite(interval) ? (std::max)(0.25, (std::min)(60.0, double(interval))) : 2.0;
    m_frame.fill(0); m_calls.fill(0); m_depth.fill(0);
    m_frameStart = ClockSeconds();
    if(!m_windowStart) m_windowStart = m_frameStart;
    m_open = true; ++m_generation;
    if(m_pendingReport >= 0) { Add(Metric::ProfileReport, m_pendingReport); m_pendingReport = -1; }
    if(m_hasPreviousStart) Add(Metric::FrameInterval, (m_frameStart - m_previousStart) * 1000.0);
    m_previousStart = m_frameStart; m_hasPreviousStart = true;
    if(gameDelta < 0 && std::isfinite(gameTime) && m_hasGameTime) gameDelta = gameTime - m_gameTime;
    if(std::isfinite(gameTime)) { m_gameTime = gameTime; m_hasGameTime = true; }
    if(std::isfinite(gameDelta) && gameDelta >= 0) Add(Metric::GameDelta, gameDelta * 1000.0);
}
void Collector::Add(Metric metric, double value, std::uint32_t calls)
{
    if(!Enabled() || !Valid(metric) || IsGpuMetric(metric) || !std::isfinite(value) || value < 0) return;
    const auto index = static_cast<std::size_t>(metric);
    if(!std::isfinite(m_frame[index] + value)) return;
    m_frame[index] += value;
    m_calls[index] = SumCalls(m_calls[index], calls);
}
void Collector::Set(Metric metric, double value)
{
    if(!Enabled() || !Valid(metric) || IsGpuMetric(metric) || !std::isfinite(value) || value < 0) return;
    const auto index = static_cast<std::size_t>(metric);
    m_frame[index] = value; m_calls[index] = 1;
}
bool Collector::EnterScope(Metric metric)
{
    if(!Enabled() || !Valid(metric) || IsGpuMetric(metric)) return false;
    auto &depth = m_depth[static_cast<std::size_t>(metric)];
    return depth++ == 0;
}
bool Collector::LeaveScope(Metric metric)
{
    if(!Enabled() || !Valid(metric) || IsGpuMetric(metric)) return false;
    auto &depth = m_depth[static_cast<std::size_t>(metric)];
    return depth && --depth == 0;
}
void Collector::EndFrame()
{
    if(!Enabled()) return;
    Set(Metric::FrameCpu, (ClockSeconds() - m_frameStart) * 1000.0);
    for(std::size_t i = 0; i < kMetricCount; ++i)
    {
        if(IsGpuMetric(static_cast<Metric>(i))) continue;
        if((i == static_cast<std::size_t>(Metric::FrameInterval) || i == static_cast<std::size_t>(Metric::GameDelta)) && !m_calls[i]) continue;
        m_series[i].Add(m_frame[i], m_calls[i]);
    }
    m_open = false;
}
void Collector::AddGpuFrame(std::uint64_t session, const std::array<double, kGpuCount> &ms,
    const std::array<std::uint32_t, kGpuCount> &calls, bool partial)
{
    if(!m_mode || session != m_session) return;
    for(std::size_t i = 0; i < kGpuCount; ++i)
    {
        // Keep the complete frame measurement if the bounded detail pool filled.
        // Do not publish underestimated component times for a partial frame.
        if(partial && i != 0) continue;
        if(std::isfinite(ms[i]) && ms[i] >= 0) m_series[kGpuFirst + i].Add(ms[i], calls[i]);
    }
}
Statistics Collector::Stats(Metric metric) const
{
    Statistics out;
    if(!Valid(metric)) return out;
    const Series &series = m_series[static_cast<std::size_t>(metric)];
    if(!series.size) return out;
    std::array<double, kSampleCapacity> sorted;
    std::copy_n(series.values.begin(), series.size, sorted.begin());
    std::sort(sorted.begin(), sorted.begin() + series.size);
    const auto percentile = [&](double fraction) {
        const std::size_t rank = static_cast<std::size_t>(std::ceil(fraction * series.size));
        return sorted[rank ? rank - 1 : 0];
    };
    out.samples = series.size; out.mean = series.sum / series.size; out.calls = series.callSum;
    out.p50 = percentile(0.50); out.p95 = percentile(0.95); out.p99 = percentile(0.99);
    out.peak = sorted[series.size - 1];
    return out;
}
bool Collector::ReportDue() const { return m_mode && !m_open && ClockSeconds() - m_windowStart >= m_interval; }
void Collector::Report(const char *side, PrintLine print)
{
    if(!m_mode || m_open || !print) return;
    const double start = ClockSeconds();
    char line[512];
    ++m_report;
    if(m_mode == 2)
        print("PERF_K11411_HEADER,side,window,metric,unit,samples,mean,p50,p95,p99,max,calls");
    else
    {
        std::snprintf(line, sizeof(line), "[perf K.11.4.1.1 %s] window=%llu CPU scopes inclusive; GPU samples delayed; percentiles use latest <=2048 samples", side, static_cast<unsigned long long>(m_report));
        print(line);
    }
    for(std::size_t i = 0; i < kMetricCount; ++i)
    {
        const auto metric = static_cast<Metric>(i);
        const auto stats = Stats(metric);
        if(!stats.samples || (!stats.calls && stats.peak == 0)) continue;
        if(m_mode == 2)
            std::snprintf(line, sizeof(line), "PERF_K11411,%s,%llu,%s,%s,%zu,%.6f,%.6f,%.6f,%.6f,%.6f,%llu", side, static_cast<unsigned long long>(m_report), MetricName(metric), MetricUnit(metric), stats.samples, stats.mean, stats.p50, stats.p95, stats.p99, stats.peak, static_cast<unsigned long long>(stats.calls));
        else
            std::snprintf(line, sizeof(line), "[perf %s] %s %s: mean=%.3f p50=%.3f p95=%.3f p99=%.3f max=%.3f samples=%zu calls=%llu", side, MetricName(metric), MetricUnit(metric), stats.mean, stats.p50, stats.p95, stats.p99, stats.peak, stats.samples, static_cast<unsigned long long>(stats.calls));
        print(line);
    }
    for(auto &series : m_series) series.Clear();
    m_windowStart = ClockSeconds();
    m_pendingReport = (m_windowStart - start) * 1000.0;
}
}
