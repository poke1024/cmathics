#ifndef CMATHICS_ASSIGNMENT_H
#define CMATHICS_ASSIGNMENT_H

#include "../core/runtime.h"

namespace Builtin {

	class Assignment : public Unit {
	public:
		Assignment(Runtime &runtime) : Unit(runtime) {
		}

		void initialize();
	};

} // end namespace Builtin

#endif //CMATHICS_ASSIGNMENT_H
