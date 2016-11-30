#ifndef CMATHICS_FUNCTIONAL_H
#define CMATHICS_FUNCTIONAL_H

#include "../core/runtime.h"

namespace Builtin {

	class Functional : public Unit {
	public:
		Functional(Runtime &runtime) : Unit(runtime) {
		}

		void initialize();
	};

} // end namespace Builtin

#endif //CMATHICS_FUNCTIONAL_H
