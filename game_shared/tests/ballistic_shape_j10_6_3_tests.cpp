// Production selection policy; actual SDK geometry is a separate acceptance check.
#include "ballistic_shape_selection.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <limits>

int main() {
    using namespace modular;
    std::array<BallisticEntryCandidate,3> entries{{{0,0.12f},{1,0.06f},{2,0.0f}}};
    std::array<int,3> order{{0,1,2}};
    int permutations=0;
    do {
        const BallisticEntryCandidate input[]={entries[order[0]],entries[order[1]],entries[order[2]]};
        const auto selected=SelectBallisticEntry(input,3);
        assert(selected.index==2 && selected.distance==0.0f);
        assert(!ValidBallisticEntrySelection(selected));
        ++permutations;
    } while(std::next_permutation(order.begin(),order.end()));
    assert(permutations==6);
    const float tolerance=0.075f;
    for(float offset:{std::nextafter(tolerance,0.0f),tolerance,
                     std::nextafter(tolerance,1.0f)}) {
        BallisticEntryCandidate input[]={{9,0.0f},{3,offset}};
        for(int reverse=0;reverse<2;++reverse) {
            const auto selected=SelectBallisticEntry(input,2);
            assert(selected.index==9);
            assert(ValidBallisticEntrySelection(selected)==(offset>tolerance));
            std::swap(input[0],input[1]);
        }
    }
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    const BallisticEntryCandidate invalid[]={{-1,0},{0,nan},{1,inf},{2,-0.01f},{3,0.751f}};
    assert(!ValidBallisticEntrySelection(SelectBallisticEntry(invalid,5)));
    assert(!ValidBallisticEntrySelection(SelectBallisticEntry(nullptr,0)));
    assert(!ValidBallisticEntrySelection(SelectBallisticEntry(invalid,5,nan)));
    assert(!ValidBallisticEntrySelection(SelectBallisticEntry(invalid,5,-1)));
    const BallisticEntryCandidate bounds[]={{8,0.75f},{7,0.0f}};
    assert(SelectBallisticEntry(bounds,1).index==8);
    assert(SelectBallisticEntry(bounds,2).index==7);
    // Multiple references carry the same shape identity and do not create ambiguity.
    const BallisticEntryCandidate duplicate[]={{5,0.06f},{5,0.0f},{8,0.12f},{5,0.0f}};
    const auto selected=SelectBallisticEntry(duplicate,4);
    assert(ValidBallisticEntrySelection(selected) && selected.index==5);
    const BallisticEntryCandidate distinct[]={{5,0},{8,0.06f},{5,0}};
    assert(!ValidBallisticEntrySelection(SelectBallisticEntry(distinct,3)));
    std::cout << "PASS J.10.6.3 shape policy: six orders, tolerance boundaries, invalid values, identities\n";
}
