#include "types.h"
#include "heap.h"
#include "core/atoms/integer.h"
#include "core/expression/implementation.h"
#include "definitions.h"
#include "core/pattern/matcher.tcc"

LegacyPool *LegacyPool::_s_instance = nullptr;

// try Timing[Length[Table[x, {x, 0, 10000000}]]] to see test the speed of pool allocations.

void LegacyPool::init() {
    // we assume: no other threads are running yet; otherwise setting
    // _default_match would be unsafe.

    assert(_s_instance == nullptr);
    _s_instance = new LegacyPool();
}
