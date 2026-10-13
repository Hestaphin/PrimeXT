#ifndef PRIMEXT_PERFORMANCE_CACHE_H
#define PRIMEXT_PERFORMANCE_CACHE_H

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

namespace performance
{
inline constexpr std::uint32_t kPrimitivePose = 0x80000000u;
// Equality, rather than a hash alone, decides whether collision geometry may
// be reused. Every matrix is the actual source used to construct that mesh.
struct TracePose
{
    std::uint32_t section = 0;
    std::array<float, 16> matrix{};
    bool operator==(const TracePose &other) const
    { return section == other.section && matrix == other.matrix; }
};

template<class Matrix>
inline TracePose MakeTracePose(std::uint32_t section, const Matrix &matrix)
{
    TracePose pose;
    pose.section = section;
    float values[16];
    matrix.CopyToArray(values);
    for(int i = 0; i < 16; ++i) pose.matrix[i] = values[i];
    return pose;
}

struct TraceMeshKey
{
    const void *model = nullptr;
    const void *definition = nullptr;
    std::uint32_t modelCRC = 0;
    std::uint64_t fingerprint = 0;
    int body = 0, skin = 0;
    std::array<float, 3> origin{}, angles{}, scale{};
    std::vector<TracePose> poses;
    bool valid = false;

    bool operator==(const TraceMeshKey &other) const
    {
        return valid && other.valid && model == other.model &&
            definition == other.definition && modelCRC == other.modelCRC && fingerprint == other.fingerprint &&
            body == other.body && skin == other.skin && origin == other.origin &&
            angles == other.angles && scale == other.scale && poses == other.poses;
    }
};

inline bool FiniteTraceKey(const TraceMeshKey &key)
{
    for(int i = 0; i < 3; ++i)
        if(!std::isfinite(key.origin[i]) || !std::isfinite(key.angles[i]) ||
            !std::isfinite(key.scale[i]) || key.scale[i] == 0.0f) return false;
    for(const TracePose &pose : key.poses)
        for(float value : pose.matrix) if(!std::isfinite(value)) return false;
    return true;
}

inline const TracePose *FindTracePose(const TraceMeshKey &key, std::uint32_t section)
{
    for(const TracePose &pose : key.poses) if(pose.section == section) return &pose;
    return nullptr;
}

struct ClientCounters
{
    std::uint64_t predictionBuilds = 0, predictionHits = 0;
    std::uint64_t boneEvaluations = 0, boneHits = 0, bonePublications = 0;
    std::uint64_t uniformUploads = 0, uniformHits = 0;
};

inline ClientCounters &ClientStats()
{
    static ClientCounters counters;
    return counters;
}

// Disabled timers make no clock calls. Body/mesh counters remain available
// without enabling diagnostic output or per-frame console logging.
class ScopedTimer
{
public:
    ScopedTimer(double &milliseconds, bool enabled) : m_output(enabled ? &milliseconds : nullptr)
    { if(m_output) m_start = std::chrono::steady_clock::now(); }
    ~ScopedTimer()
    {
        if(m_output) *m_output += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - m_start).count();
    }
private:
    double *m_output;
    std::chrono::steady_clock::time_point m_start;
};

class ScopedPeakTimer
{
public:
    ScopedPeakTimer(double &last, double &peak, std::uint64_t &requests, bool enabled)
        : m_last(last), m_peak(peak), m_requests(requests), m_enabled(enabled)
    { if(m_enabled) m_start = std::chrono::steady_clock::now(); }
    ~ScopedPeakTimer()
    {
        if(!m_enabled) return;
        m_last = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_start).count();
        if(m_last > m_peak) m_peak = m_last;
        ++m_requests;
    }
private:
    double &m_last, &m_peak;
    std::uint64_t &m_requests;
    bool m_enabled;
    std::chrono::steady_clock::time_point m_start;
};
}

#endif
