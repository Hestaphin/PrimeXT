#include "performance_gpu.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

extern "C" const char *__asan_default_options() { return "detect_leaks=0:halt_on_error=1"; }

namespace
{
double now = 100;
unsigned clocks = 0;
std::vector<std::string> lines;
void Print(const char *line) { lines.emplace_back(line); }
bool Near(double a, double b) { return std::abs(a - b) < 1e-6; }
void Reset() { pxperf::Get().Reset(); now = 100; clocks = 0; lines.clear(); }
void Begin(float mode = 1, double time = 10) { pxperf::Get().BeginFrame(mode, 2, time, .01); }
struct Driver : pxperf::TimestampBackend
{
    struct Query { std::uint64_t stamp = 0; bool ready = false; };
    std::array<Query, pxperf::GpuRecorder::kQueries + 1> queries{};
    unsigned create = 0, destroy = 0, stamps = 0, polls = 0, reads = 0;
    std::uint64_t tick = 0;
    bool fail = false;
    bool Create(std::size_t count, std::uint32_t *ids) override
    {
        ++create;
        for(std::size_t i = 0; i < count; ++i) ids[i] = static_cast<std::uint32_t>(i + 1);
        return !fail;
    }
    void Destroy(std::size_t count, const std::uint32_t *) override { assert(count == pxperf::GpuRecorder::kQueries); ++destroy; }
    void Stamp(std::uint32_t id) override { ++stamps; queries[id] = {tick += 1000000, false}; }
    bool Available(std::uint32_t id) override { ++polls; return queries[id].ready; }
    std::uint64_t Read(std::uint32_t id) override { assert(queries[id].ready); ++reads; return queries[id].stamp; }
    void Ready() { for(auto &query : queries) query.ready = true; }
};
void Disabled()
{
    Reset(); Begin(0);
    { pxperf::Scope scope(pxperf::Metric::Physics); pxperf::Get().Add(pxperf::Metric::PhysicsSteps, 1); }
    pxperf::Get().EndFrame();
    assert(clocks == 0 && pxperf::Get().Stats(pxperf::Metric::Physics).samples == 0);
    Driver driver; pxperf::GpuRecorder gpu;
    assert(gpu.Initialize(&driver)); gpu.BeginFrame(false);
    const auto token = gpu.Begin(pxperf::Metric::GpuShadows); gpu.End(token); gpu.EndFrame();
    assert(driver.stamps == 0 && driver.polls == 0 && driver.reads == 0);
    gpu.Shutdown(); assert(driver.destroy == 1);
}
void NestedScopes()
{
    Reset(); Begin();
    {
        pxperf::Scope outer(pxperf::Metric::CollisionQuery); now += .002;
        { pxperf::Scope inner(pxperf::Metric::CollisionQuery); now += .003; }
        { pxperf::Scope other(pxperf::Metric::ImpactMesh); now += .004; }
    }
    pxperf::Get().EndFrame();
    const auto trace = pxperf::Get().Stats(pxperf::Metric::CollisionQuery);
    assert(Near(trace.mean, 9) && trace.calls == 2);
    assert(Near(pxperf::Get().Stats(pxperf::Metric::ImpactMesh).mean, 4));
    assert(Near(pxperf::Get().Stats(pxperf::Metric::FrameCpu).mean, 9));
}
void Percentiles()
{
    Reset();
    for(int i = 1; i <= 100; ++i)
    {
        Begin(1, 10 + i * .01);
        pxperf::Get().Add(pxperf::Metric::Physics, i);
        pxperf::Get().Set(pxperf::Metric::PhysicsSteps, i % 3);
        now += .001; pxperf::Get().EndFrame(); now += .009;
    }
    const auto stats = pxperf::Get().Stats(pxperf::Metric::Physics);
    assert(stats.samples == 100 && stats.calls == 100 && Near(stats.mean, 50.5));
    assert(stats.p50 == 50 && stats.p95 == 95 && stats.p99 == 99 && stats.peak == 100);
    assert(pxperf::Get().Stats(pxperf::Metric::FrameInterval).samples == 99);
    assert(Near(pxperf::Get().Stats(pxperf::Metric::FrameInterval).mean, 10));
}
void Capacity()
{
    Reset();
    const int total = static_cast<int>(pxperf::kSampleCapacity) + 17;
    for(int i = 1; i <= total; ++i)
    {
        Begin(1, 10 + i * .01); pxperf::Get().Add(pxperf::Metric::Physics, i, 2);
        pxperf::Get().EndFrame(); now += .01;
    }
    const auto stats = pxperf::Get().Stats(pxperf::Metric::Physics);
    assert(stats.samples == pxperf::kSampleCapacity && stats.calls == pxperf::kSampleCapacity * 2);
    assert(Near(stats.mean, (18.0 + total) / 2));
}
void ResetAndInvalidInputs()
{
    Reset(); Begin();
    auto stale = std::make_unique<pxperf::Scope>(pxperf::Metric::Physics);
    pxperf::Get().Add(pxperf::Metric::Physics, 5);
    const auto session = pxperf::Get().Session();
    now += .1; Begin(1, 2); // game clock rewind resets all old samples/scopes
    assert(pxperf::Get().Session() != session);
    stale.reset();
    pxperf::Get().Add(pxperf::Metric::Physics, -1);
    pxperf::Get().Add(pxperf::Metric::Physics, std::numeric_limits<double>::infinity());
    pxperf::Get().Add(pxperf::Metric::Count, 1);
    pxperf::Get().Set(pxperf::Metric::PhysicsSteps, std::numeric_limits<double>::quiet_NaN());
    pxperf::Get().EndFrame();
    assert(pxperf::Get().Stats(pxperf::Metric::Physics).calls == 0);
    Begin(std::numeric_limits<float>::quiet_NaN()); assert(!pxperf::Get().Enabled());
    assert(pxperf::Get().Stats(pxperf::Metric::Physics).samples == 0);
    pxperf::Get().BeginFrame(1, std::numeric_limits<float>::infinity(), 5, -1);
    pxperf::Get().EndFrame();
    assert(pxperf::Get().Stats(pxperf::Metric::GameDelta).samples == 0);
    now += .01; pxperf::Get().BeginFrame(1, 2, 5.02, -1); pxperf::Get().EndFrame();
    assert(Near(pxperf::Get().Stats(pxperf::Metric::GameDelta).mean, 20));
}
void AsyncGpu()
{
    Reset(); Driver driver; pxperf::GpuRecorder gpu;
    assert(gpu.Initialize(&driver)); Begin(); gpu.BeginFrame(true);
    const auto shadow = gpu.Begin(pxperf::Metric::GpuShadows);
    const auto nested = gpu.Begin(pxperf::Metric::GpuShadows); gpu.End(nested);
    gpu.End(shadow); gpu.EndFrame();
    pxperf::Get().EndFrame();
    Begin(1, 10.01); gpu.BeginFrame(true); gpu.EndFrame(); pxperf::Get().EndFrame();
    assert(driver.reads == 0 && pxperf::Get().Stats(pxperf::Metric::GpuFrame).samples == 0);
    driver.Ready(); Begin(1, 10.02); gpu.BeginFrame(true);
    assert(driver.reads == 6);
    assert(pxperf::Get().Stats(pxperf::Metric::GpuFrame).samples == 2);
    assert(Near(pxperf::Get().Stats(pxperf::Metric::GpuShadows).mean, .5));
    assert(pxperf::Get().Stats(pxperf::Metric::GpuShadows).calls == 2);
    gpu.EndFrame(); pxperf::Get().EndFrame(); gpu.Shutdown();
}
void BusyGpu()
{
    Reset(); Driver driver; pxperf::GpuRecorder gpu; assert(gpu.Initialize(&driver));
    for(std::size_t i = 0; i <= pxperf::GpuRecorder::kFrames; ++i)
    {
        Begin(1, 10 + i * .01); gpu.BeginFrame(true); gpu.EndFrame(); pxperf::Get().EndFrame();
    }
    assert(driver.stamps == pxperf::GpuRecorder::kFrames * 2 && driver.reads == 0);
    assert(pxperf::Get().Stats(pxperf::Metric::GpuSkippedFrames).peak == 1);
    driver.Ready(); Begin(1, 11); gpu.BeginFrame(true);
    assert(pxperf::Get().Stats(pxperf::Metric::GpuFrame).samples == pxperf::GpuRecorder::kFrames);
    gpu.EndFrame(); pxperf::Get().EndFrame(); gpu.Shutdown();
}
void PartialGpuAndStaleTokens()
{
    Reset(); Driver driver; pxperf::GpuRecorder gpu; assert(gpu.Initialize(&driver));
    Begin(); gpu.BeginFrame(true);
    for(std::size_t i = 0; i < pxperf::GpuRecorder::kPairs + 5; ++i)
    {
        const auto token = gpu.Begin(pxperf::Metric::GpuShadows); gpu.End(token);
    }
    gpu.EndFrame(); pxperf::Get().EndFrame(); driver.Ready();
    Begin(1, 10.01); gpu.BeginFrame(true);
    assert(pxperf::Get().Stats(pxperf::Metric::GpuFrame).samples == 1);
    assert(pxperf::Get().Stats(pxperf::Metric::GpuShadows).samples == 0);
    const auto stale = gpu.Begin(pxperf::Metric::GpuWorld);
    const auto before = driver.stamps;
    gpu.Reset(); gpu.End(stale); assert(driver.stamps == before);
    pxperf::Get().EndFrame(); gpu.Shutdown();
}
void GpuResetAndBadTimestamps()
{
    Reset(); Driver driver; pxperf::GpuRecorder gpu; assert(gpu.Initialize(&driver));
    Begin(); gpu.BeginFrame(true); gpu.EndFrame(); pxperf::Get().EndFrame(); driver.Ready();
    pxperf::Get().Reset(); Begin(1, 2); gpu.BeginFrame(true);
    assert(driver.reads == 0 && pxperf::Get().Stats(pxperf::Metric::GpuFrame).samples == 0);
    gpu.EndFrame(); pxperf::Get().EndFrame(); driver.Ready();
    driver.queries[2].stamp = 0;
    Begin(1, 2.01); gpu.BeginFrame(true); gpu.EndFrame(); pxperf::Get().EndFrame();
    assert(pxperf::Get().Stats(pxperf::Metric::GpuInvalidFrames).peak == 1);
    gpu.Shutdown();
    driver.fail = true; assert(!gpu.Initialize(&driver) && !gpu.Supported());
    assert(driver.destroy == 2);
}
void CsvAndWindow()
{
    Reset(); pxperf::Get().BeginFrame(2, .1f, 10, .01);
    pxperf::Get().Add(pxperf::Metric::Physics, 3.5, 2);
    now += .01; pxperf::Get().EndFrame(); assert(!pxperf::Get().ReportDue());
    now += .3; assert(pxperf::Get().ReportDue()); pxperf::Get().Report("server", Print);
    assert(lines[0].find("PERF_K11411_HEADER,") == 0);
    bool found = false;
    for(const auto &line : lines)
        if(line.find(",physics,ms,1,3.500000,3.500000,3.500000,3.500000,3.500000,2") != std::string::npos) found = true;
    assert(found && pxperf::Get().Stats(pxperf::Metric::Physics).samples == 0);
    assert(!pxperf::Get().ReportDue());
}
}
namespace pxperf { double ClockSeconds() { ++clocks; return now; } }
int main()
{
    Disabled(); NestedScopes(); Percentiles(); Capacity(); ResetAndInvalidInputs();
    AsyncGpu(); BusyGpu(); PartialGpuAndStaleTokens(); GpuResetAndBadTimestamps(); CsvAndWindow();
    std::cout << "PASS 10 profiling groups: disabled clocks, nesting, percentiles, capacity, timelines, asynchronous GPU, busy pool, partial capture, lifecycle, CSV\n";
}
