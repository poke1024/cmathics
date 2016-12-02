#ifndef CMATHICS_FUNCTIONAL_H
#define CMATHICS_FUNCTIONAL_H

#include "../core/runtime.h"

namespace Builtins {

	class Functional : public Unit {
	public:
		Functional(Runtime &runtime) : Unit(runtime) {
		}

		void initialize();
	};

} // end namespace Builtins

#endif //CMATHICS_FUNCTIONAL_H
