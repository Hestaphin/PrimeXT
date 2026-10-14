#include "hud.h"
#include "utils.h"
#include "gl_local.h"
#include "gl_cvars.h"
#include "gl_performance.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
constexpr GLenum kTimestamp = 0x8E28, kCounterBits = 0x8864;
constexpr GLenum kResult = 0x8866, kResultAvailable = 0x8867;
template<typename T> T LoadProc(const char *name, const char *fallback = nullptr)
{
    void *address = reinterpret_cast<void *>(GL_GetProcAddress(name));
    if(!address && fallback) address = reinterpret_cast<void *>(GL_GetProcAddress(fallback));
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    if(value <= 3 || value == ~std::uintptr_t(0)) return nullptr;
    return reinterpret_cast<T>(address);
}
bool HasExtension(const char *extensions, const char *name)
{
    if(!extensions || !name || !*name) return false;
    const auto length = std::strlen(name);
    for(const char *hit = extensions; (hit = std::strstr(hit, name)) != nullptr; hit += length)
        if((hit == extensions || hit[-1] == ' ') && (hit[length] == 0 || hit[length] == ' ')) return true;
    return false;
}
class GlTimestamps final : public pxperf::TimestampBackend
{
public:
    using Gen = void (APIENTRY *)(GLsizei, GLuint *);
    using Delete = void (APIENTRY *)(GLsizei, const GLuint *);
    using Counter = void (APIENTRY *)(GLuint, GLenum);
    using GetQuery = void (APIENTRY *)(GLenum, GLenum, GLint *);
    using GetObject = void (APIENTRY *)(GLuint, GLenum, GLint *);
    using GetTime = void (APIENTRY *)(GLuint, GLenum, unsigned long long *);
    bool Load()
    {
        if(gl_extensions && !CVAR_TO_BOOL(gl_extensions)) return false;
        int major = 0, minor = 0;
        if(glConfig.version_string) std::sscanf(glConfig.version_string, "%d.%d", &major, &minor);
        if(!(major > 3 || (major == 3 && minor >= 3)) &&
            !HasExtension(glConfig.extensions_string, "GL_ARB_timer_query")) return false;
        gen = LoadProc<Gen>("glGenQueries", "glGenQueriesARB");
        destroy = LoadProc<Delete>("glDeleteQueries", "glDeleteQueriesARB");
        counter = LoadProc<Counter>("glQueryCounter");
        query = LoadProc<GetQuery>("glGetQueryiv", "glGetQueryivARB");
        object = LoadProc<GetObject>("glGetQueryObjectiv", "glGetQueryObjectivARB");
        time = LoadProc<GetTime>("glGetQueryObjectui64v");
        if(!gen || !destroy || !counter || !query || !object || !time) return false;
        GLint bits = 0;
        query(kTimestamp, kCounterBits, &bits);
        // Conservative support: no ambiguous short-counter rollover samples.
        return bits == 64;
    }
    bool Create(std::size_t count, std::uint32_t *ids) override
    {
        gen(static_cast<GLsizei>(count), ids);
        for(std::size_t i = 0; i < count; ++i) if(!ids[i]) return false;
        return true;
    }
    void Destroy(std::size_t count, const std::uint32_t *ids) override { if(destroy) destroy(static_cast<GLsizei>(count), ids); }
    void Stamp(std::uint32_t id) override { counter(id, kTimestamp); }
    bool Available(std::uint32_t id) override { GLint ready = 0; object(id, kResultAvailable, &ready); return ready != 0; }
    std::uint64_t Read(std::uint32_t id) override { unsigned long long value = 0; time(id, kResult, &value); return value; }
private:
    Gen gen = nullptr;
    Delete destroy = nullptr;
    Counter counter = nullptr;
    GetQuery query = nullptr;
    GetObject object = nullptr;
    GetTime time = nullptr;
};
GlTimestamps backend;
bool attempted = false;
std::uint64_t announcedSession = 0;
void Print(const char *line) { ALERT(at_console, "%s\n", line); }
}
void R_ProfileInitialize()
{
    pxperf::Get().Reset(); pxperf::Gpu().Reset();
    attempted = false; announcedSession = 0;
}
void R_ProfileShutdown()
{
    pxperf::Gpu().Shutdown(); pxperf::Get().Reset();
    attempted = false; announcedSession = 0;
}
void R_ProfileBeginFrame()
{
    pxperf::Get().BeginFrame(r_profile ? r_profile->value : 0,
        r_profile_interval ? r_profile_interval->value : 2, GET_CLIENT_TIME(), -1);
    const bool requestGpu = pxperf::Get().Enabled() && r_profile_gpu &&
        std::isfinite(r_profile_gpu->value) && r_profile_gpu->value > 0;
    if(requestGpu && !attempted)
    {
        attempted = true;
        if(backend.Load()) pxperf::Gpu().Initialize(&backend);
    }
    if(pxperf::Get().Enabled() && announcedSession != pxperf::Get().Session())
    {
        announcedSession = pxperf::Get().Session();
        Print(requestGpu ? (pxperf::Gpu().Supported() ?
            "[perf client] GPU timestamp queries enabled (64-bit; asynchronous)." :
            "[perf client] GPU timestamps unavailable; CPU timings remain available.") :
            "[perf client] GPU measurement disabled; CPU timings enabled.");
    }
    pxperf::Gpu().BeginFrame(requestGpu);
}
void R_ProfileEndFrame()
{
    pxperf::Gpu().EndFrame();
    pxperf::Get().EndFrame();
    if(pxperf::Get().ReportDue()) pxperf::Get().Report("client", Print);
}
