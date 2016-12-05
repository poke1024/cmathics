#include "types.h"
#include "heap.h"
#include "integer.h"
#include "leaves.h"
#include "expression.h"
#include "definitions.h"
#include "matcher.h"

Heap *Heap::_s_instance = nullptr;


Heap::Heap() :
    // specify number of objects to allocate with each system malloc;
    // try Timing[Length[Table[x, {x, 0, 10000000}]]] to see how the
    // heap allocations get very slow if these values are too small.

    _machine_integers(4096),
    _machine_reals(4096),

    _static_expression_heap(512),
    _dynamic_expressions(512) {
}

void Heap::init() {
    assert(_s_instance == nullptr);
    _s_instance = new Heap();
}
