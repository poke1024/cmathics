#ifndef CMATHICS_NUMERIC_H
#define CMATHICS_NUMERIC_H

#include "../core/runtime.h"

namespace Builtins {

	class Numeric : public Unit {
	public:
		Numeric(Runtime &runtime) : Unit(runtime) {
		}

		void initialize();
	};

} // end namespace Builtins

#endif //CMATHICS_NUMERIC_H
