#include <array>
#include <cstddef>
#include <cstdint>
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#include "performance_profile.h"
#include "performance_gpu.h"
extern "C" const char *__asan_default_options() { return "detect_leaks=0:halt_on_error=1"; }
int main()
{
    pxperf::Scope disabled(pxperf::Metric::Physics);
    pxperf::GpuScope disabledGpu(pxperf::Metric::GpuFrame);
    return pxperf::Get().Enabled() ? 1 : 0;
}
