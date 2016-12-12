#ifndef CMATHICS_STRINGS_H
#define CMATHICS_STRINGS_H

#include "../core/runtime.h"

namespace Builtins {

	class Strings : public Unit {
	public:
		Strings(Runtime &runtime) : Unit(runtime) {
		}

		void initialize();
	};

} // end namespace Builtins

#endif //CMATHICS_STRINGS_H
