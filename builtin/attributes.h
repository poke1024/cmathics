#ifndef CMATHICS_ATTRIBUTES_H
#define CMATHICS_ATTRIBUTES_H

#include "../core/runtime.h"

namespace Builtins {

	class Attributes : public Unit {
	public:
		Attributes(Runtime &runtime) : Unit(runtime) {
		}

		void initialize();
	};

} // end namespace Builtins

#endif //CMATHICS_ATTRIBUTES_H
