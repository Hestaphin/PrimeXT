#pragma once
#include "performance_profile.h"

namespace pxperf
{
// Small recording interface: production uses GL timestamps; tests use a
// delayed driver. This collector never waits, flushes or finishes a GPU queue.
struct TimestampBackend
{
    virtual ~TimestampBackend() = default;
    virtual bool Create(std::size_t count, std::uint32_t *ids) = 0;
    virtual void Destroy(std::size_t count, const std::uint32_t *ids) = 0;
    virtual void Stamp(std::uint32_t id) = 0;
    virtual bool Available(std::uint32_t id) = 0;
    virtual std::uint64_t Read(std::uint32_t id) = 0;
};
struct GpuToken
{
    std::uint64_t serial = 0;
    int frame = -1, pair = -1;
};
class GpuRecorder
{
public:
    static constexpr std::size_t kFrames = 8, kPairs = 64;
    static constexpr std::size_t kQueries = kFrames * kPairs * 2;
    bool Initialize(TimestampBackend *backend);
    void Shutdown(); // Must run while the owning GL context still exists.
    void Reset();    // Invalidate pending samples without issuing GL commands.
    bool Supported() const { return m_backend != nullptr; }
    void BeginFrame(bool enabled);
    void EndFrame();
    GpuToken Begin(Metric metric);
    void End(GpuToken token);
private:
    enum class State { Free, Recording, Pending };
    struct Pair { Metric metric = Metric::GpuFrame; bool closed = false; std::uint32_t calls = 1; };
    struct Frame
    {
        State state = State::Free;
        std::uint64_t serial = 0, session = 0;
        std::size_t count = 0;
        bool partial = false;
        std::array<Pair, kPairs> pairs{};
    };
    void Poll();
    std::uint32_t Query(std::size_t frame, std::size_t pair, bool end) const;
    TimestampBackend *m_backend = nullptr;
    std::array<std::uint32_t, kQueries> m_ids{};
    std::array<Frame, kFrames> m_frames{};
    int m_active = -1;
    std::size_t m_next = 0;
    std::uint64_t m_serial = 0, m_session = 0;
};
GpuRecorder &Gpu();
class GpuScope
{
public:
    explicit GpuScope(Metric metric) : m_token(Gpu().Begin(metric)) {}
    ~GpuScope() { Gpu().End(m_token); }
    GpuScope(const GpuScope &) = delete;
    GpuScope &operator=(const GpuScope &) = delete;
private:
    GpuToken m_token;
};
}
