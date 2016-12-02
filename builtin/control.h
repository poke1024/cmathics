#ifndef CMATHICS_CONTROL_H
#define CMATHICS_CONTROL_H

#include "../core/runtime.h"

namespace Builtins {

	class Control : public Unit {
	public:
		Control(Runtime &runtime) : Unit(runtime) {
		}

		void initialize();
	};

} // end namespace Builtins

#endif //CMATHICS_CONTROL_H
