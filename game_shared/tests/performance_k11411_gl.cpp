#include "hud.h"
#include "utils.h"
#include "gl_local.h"
#include "gl_cvars.h"
#include "gl_performance.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#undef GL_GetProcAddress
extern "C" const char *__asan_default_options() { return "detect_leaks=0:halt_on_error=1"; }

cl_enginefunc_t gEngfuncs{};
render_api_t gRenderfuncs{};
glConfig_t glConfig{};
static cvar_t mode{}, interval{}, gpuMode{}, extensions{};
cvar_t *r_profile = &mode, *r_profile_interval = &interval, *r_profile_gpu = &gpuMode;
cvar_t *gl_extensions = &extensions;
static double wall = 100;
static float gameTime = 10;
static unsigned stamps = 0, creates = 0, deletes = 0, queryBits = 0, reads = 0;
static GLint bits = 64;
static std::array<unsigned long long, pxperf::GpuRecorder::kQueries + 1> times{};
static std::vector<std::string> output;
void ALERT(ALERT_TYPE, char *format, ...)
{
    char line[1024]; va_list args; va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args); va_end(args); output.emplace_back(line);
}
static float ClientTime() { return gameTime; }
static void APIENTRY Gen(GLsizei count, GLuint *ids)
{
    ++creates;
    for(GLsizei i = 0; i < count; ++i) ids[i] = static_cast<GLuint>(i + 1);
}
static void APIENTRY Delete(GLsizei, const GLuint *) { ++deletes; }
static void APIENTRY Counter(GLuint id, GLenum target) { assert(target == 0x8E28); times[id] = ++stamps * 1000000ULL; }
static void APIENTRY Query(GLenum target, GLenum name, GLint *out)
{
    assert(target == 0x8E28 && name == 0x8864); ++queryBits; *out = bits;
}
static void APIENTRY Object(GLuint, GLenum name, GLint *out) { assert(name == 0x8867); *out = 1; }
static void APIENTRY Time(GLuint id, GLenum name, unsigned long long *out) { assert(name == 0x8866); ++reads; *out = times[id]; }
static void *Proc(const char *name)
{
    if(!std::strcmp(name, "glGenQueries")) return reinterpret_cast<void *>(Gen);
    if(!std::strcmp(name, "glDeleteQueries")) return reinterpret_cast<void *>(Delete);
    if(!std::strcmp(name, "glQueryCounter")) return reinterpret_cast<void *>(Counter);
    if(!std::strcmp(name, "glGetQueryiv")) return reinterpret_cast<void *>(Query);
    if(!std::strcmp(name, "glGetQueryObjectiv")) return reinterpret_cast<void *>(Object);
    if(!std::strcmp(name, "glGetQueryObjectui64v")) return reinterpret_cast<void *>(Time);
    return nullptr;
}
namespace pxperf { double ClockSeconds() { return wall; } }
static void Frame()
{
    R_ProfileBeginFrame();
    { pxperf::GpuScope scope(pxperf::Metric::GpuShadows); }
    wall += .016; gameTime += .016f; R_ProfileEndFrame();
}
int main()
{
    gEngfuncs.GetClientTime = ClientTime; gRenderfuncs.GL_GetProcAddress = Proc;
    glConfig.version_string = "3.3"; glConfig.extensions_string = "";
    interval.value = 2; gpuMode.value = extensions.value = 1;
    R_ProfileInitialize(); Frame();
    assert(queryBits == 0 && creates == 0 && stamps == 0 && output.empty());
    mode.value = 1; Frame(); Frame();
    assert(queryBits == 1 && creates == 1 && stamps == 8 && reads == 4);
    assert(pxperf::Get().Stats(pxperf::Metric::GpuFrame).samples == 1);
    R_ProfileShutdown(); assert(deletes == 1);
    bits = 32; R_ProfileInitialize(); Frame(); assert(!pxperf::Gpu().Supported());
    assert(creates == 1 && queryBits == 2); R_ProfileShutdown();
    bits = 64; glConfig.version_string = "2.1";
    glConfig.extensions_string = "GL_ARB_timer_query_fake";
    R_ProfileInitialize(); Frame(); assert(!pxperf::Gpu().Supported());
    assert(queryBits == 2); R_ProfileShutdown();
    glConfig.extensions_string = "GL_ARB_timer_query";
    R_ProfileInitialize(); Frame(); assert(pxperf::Gpu().Supported()); R_ProfileShutdown();
    extensions.value = 0; R_ProfileInitialize(); Frame();
    assert(!pxperf::Gpu().Supported()); R_ProfileShutdown();
    extensions.value = 1; gpuMode.value = 0; R_ProfileInitialize();
    const auto before = stamps; Frame(); assert(stamps == before);
    R_ProfileShutdown();
    std::cout << "PASS real GL profiling adapter: disabled mode, core/ARB capability, short-counter fallback, exact extension tokens, extension toggle, CPU-only capture, shutdown\n";
}
