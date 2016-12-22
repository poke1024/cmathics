#include "types.h"
#include "heap.h"
#include "integer.h"
#include "leaves.h"
#include "expression.h"
#include "definitions.h"
#include "matcher.h"

/*thread_local*/ Pool *Pool::_s_instance = nullptr;


Pool::Pool() :
    // specify number of objects to allocate with each system malloc;
    // try Timing[Length[Table[x, {x, 0, 10000000}]]] to see how the
    // heap allocations get very slow if these values are too small.

    _machine_integers(4096),
    _machine_reals(4096),

    _static_expression_heap(512),
    _dynamic_expressions(512) {
}

void Pool::init() {
    assert(_s_instance == nullptr);
    _s_instance = new Pool();

	_s_instance->_default_match = _s_instance->_matches.construct();
	_s_instance->_no_symbolic_form = _s_instance->_symbolic_forms.construct(SymEngineRef(), false);
}
