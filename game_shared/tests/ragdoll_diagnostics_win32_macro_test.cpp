// K.11.2 regression: mimic Windows SDK min/max macros in a translation unit
// that has already included C++ standard headers (as the model viewer does).
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

#define min(a,b) WINDOWS_MIN_MACRO_SHOULD_NOT_BE_CALLED
#define max(a,b) WINDOWS_MAX_MACRO_SHOULD_NOT_BE_CALLED
#include "../ragdoll_diagnostics.h"

#ifndef min
#error "The diagnostics header unexpectedly modified the Windows min macro"
#endif
#ifndef max
#error "The diagnostics header unexpectedly modified the Windows max macro"
#endif

int main()
{
    using namespace ragdolldiag;
    assert(IntervalExcess(11.0, -5.0, 5.0) == 6.0);
    assert(IntervalExcess(3.0, -5.0, 5.0) == 0.0);
    ReferenceSpan span;
    span.Add({-2.0, 1.0, 8.0});
    span.Add({5.0, -4.0, 3.0});
    assert(span.present && span.Value() == 7.0);
    Snapshot a, b;
    a.maxLinearSpeed = 9.0;
    b.maxLinearSpeed = 11.0;
    Window window;
    window.Push(a);
    window.Push(b);
    const Snapshot merged = window.Take();
    assert(merged.maxLinearSpeed == 11.0 && merged.windowSamples == 2);
    assert(window.Empty());
    Cadence cadence;
    assert(cadence.Advance(0.01, 0.5));
    assert(!cadence.Advance(0.01, 0.5));
    assert(!CsvRow(merged).empty());
}
