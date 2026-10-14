#include "performance_gpu.h"
#include <limits>

namespace pxperf
{
GpuRecorder &Gpu() { static GpuRecorder recorder; return recorder; }
std::uint32_t GpuRecorder::Query(std::size_t frame, std::size_t pair, bool end) const
{
    return m_ids[(frame * kPairs + pair) * 2 + (end ? 1 : 0)];
}
bool GpuRecorder::Initialize(TimestampBackend *backend)
{
    Shutdown();
    if(!backend) return false;
    if(!backend->Create(kQueries, m_ids.data()))
    {
        backend->Destroy(kQueries, m_ids.data()); m_ids.fill(0); return false;
    }
    for(const auto id : m_ids)
        if(!id) { backend->Destroy(kQueries, m_ids.data()); m_ids.fill(0); return false; }
    m_backend = backend;
    Reset();
    return true;
}
void GpuRecorder::Shutdown()
{
    if(m_backend) m_backend->Destroy(kQueries, m_ids.data());
    m_backend = nullptr; m_ids.fill(0); Reset();
}
void GpuRecorder::Reset()
{
    for(auto &frame : m_frames) { frame.state = State::Free; frame.count = 0; frame.partial = false; }
    m_active = -1; m_next = 0; ++m_serial; m_session = Get().Session();
}
void GpuRecorder::Poll()
{
    for(std::size_t index = 0; index < kFrames; ++index)
    {
        auto &frame = m_frames[index];
        if(frame.state != State::Pending) continue;
        if(frame.session != Get().Session()) { frame.state = State::Free; continue; }
        bool ready = true;
        for(std::size_t pair = 0; pair < frame.count; ++pair)
            if(!m_backend->Available(Query(index, pair, false)) || !m_backend->Available(Query(index, pair, true)))
                { ready = false; break; }
        if(!ready) continue;
        std::array<double, kGpuCount> elapsed{};
        std::array<std::uint32_t, kGpuCount> calls{};
        bool valid = true;
        for(std::size_t pair = 0; pair < frame.count; ++pair)
        {
            const auto begin = m_backend->Read(Query(index, pair, false));
            const auto end = m_backend->Read(Query(index, pair, true));
            // The GL adapter accepts only 64-bit timestamp counters. Reject
            // reversed/reset timestamps and implausible >60 second ranges.
            if(end < begin || end - begin > 60000000000ULL) { valid = false; break; }
            const auto metric = static_cast<std::size_t>(frame.pairs[pair].metric) - kGpuFirst;
            elapsed[metric] += double(end - begin) / 1000000.0;
            calls[metric] += frame.pairs[pair].calls;
        }
        if(valid) Get().AddGpuFrame(frame.session, elapsed, calls, frame.partial);
        else Get().Add(Metric::GpuInvalidFrames, 1);
        frame.state = State::Free;
    }
}
void GpuRecorder::BeginFrame(bool enabled)
{
    if(!m_backend || !enabled || !Get().Enabled()) { Reset(); return; }
    if(m_session != Get().Session()) Reset();
    Poll();
    // An unfinished previous capture is never combined with a later frame.
    if(m_active >= 0) { Get().Add(Metric::GpuInvalidFrames, 1); Reset(); }
    for(std::size_t offset = 0; offset < kFrames; ++offset)
    {
        const auto index = (m_next + offset) % kFrames;
        auto &frame = m_frames[index];
        if(frame.state != State::Free) continue;
        frame.state = State::Recording; frame.serial = ++m_serial;
        frame.session = Get().Session(); frame.count = 0; frame.partial = false;
        m_active = static_cast<int>(index); m_next = (index + 1) % kFrames;
        Begin(Metric::GpuFrame);
        return;
    }
    Get().Add(Metric::GpuSkippedFrames, 1);
}
GpuToken GpuRecorder::Begin(Metric metric)
{
    if(m_active < 0 || !Get().Enabled() || !IsGpuMetric(metric)) return {};
    auto &frame = m_frames[static_cast<std::size_t>(m_active)];
    // Recursive subviews/shadows can enter the same category again. Keep the
    // outer timestamp range and count the call, avoiding double-counted time.
    for(std::size_t pair = 0; pair < frame.count; ++pair)
        if(frame.pairs[pair].metric == metric && !frame.pairs[pair].closed)
        {
            if(frame.pairs[pair].calls != (std::numeric_limits<std::uint32_t>::max)()) ++frame.pairs[pair].calls;
            return {};
        }
    if(frame.count == kPairs)
    {
        frame.partial = true; Get().Add(Metric::GpuDroppedScopes, 1); return {};
    }
    const auto pair = frame.count++;
    frame.pairs[pair] = {metric, false, 1};
    m_backend->Stamp(Query(static_cast<std::size_t>(m_active), pair, false));
    return {frame.serial, m_active, static_cast<int>(pair)};
}
void GpuRecorder::End(GpuToken token)
{
    if(!token.serial || token.frame < 0 || token.frame != m_active || token.pair < 0) return;
    auto &frame = m_frames[static_cast<std::size_t>(token.frame)];
    const auto pair = static_cast<std::size_t>(token.pair);
    if(frame.serial != token.serial || pair >= frame.count || frame.pairs[pair].closed) return;
    m_backend->Stamp(Query(static_cast<std::size_t>(token.frame), pair, true));
    frame.pairs[pair].closed = true;
}
void GpuRecorder::EndFrame()
{
    if(m_active < 0) return;
    auto &frame = m_frames[static_cast<std::size_t>(m_active)];
    End({frame.serial, m_active, 0});
    bool closed = true;
    for(std::size_t pair = 0; pair < frame.count; ++pair) closed &= frame.pairs[pair].closed;
    frame.state = closed ? State::Pending : State::Free;
    if(!closed) Get().Add(Metric::GpuInvalidFrames, 1);
    m_active = -1;
}
}
