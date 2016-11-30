#include "types.h"
#include "heap.h"
#include "integer.h"
#include "leaves.h"
#include "expression.h"
#include "definitions.h"
#include "matcher.h"

Heap *Heap::_s_instance = nullptr;


Heap::Heap() {
}

void Heap::init() {
    assert(_s_instance == nullptr);
    _s_instance = new Heap();
}
