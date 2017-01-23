#ifndef CMATHICS_INOUT_H
#define CMATHICS_INOUT_H

#include "../core/runtime.h"

namespace Builtins {

	class InOut : public Unit {
	public:
		InOut(Runtime &runtime) : Unit(runtime) {
		}

		void initialize();
	};

} // end namespace Builtins

#endif //CMATHICS_INOUT_H
