#include "types.h"
#include "heap.h"
#include "core/atoms/integer.h"
#include "leaves.h"
#include "core/expression/implementation.h"
#include "definitions.h"
#include "matcher.h"

Pool *Pool::_s_instance = nullptr;

// try Timing[Length[Table[x, {x, 0, 10000000}]]] to see test the speed of pool allocations.

void Pool::init() {
    // we assume: no other threads are running yet; otherwise setting
    // _default_match would be unsafe.

    assert(_s_instance == nullptr);
    _s_instance = new Pool();

	_s_instance->_default_match = _s_instance->_matches.construct();
	_s_instance->_no_symbolic_form = _s_instance->_symbolic_forms.construct(SymEngineRef());
}
