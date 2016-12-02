#ifndef CMATHICS_ARITHMETIC_H
#define CMATHICS_ARITHMETIC_H

#include "../core/runtime.h"

namespace Builtins {

class Arithmetic : public Unit {
public:
	Arithmetic(Runtime &runtime) : Unit(runtime) {
	}

	void initialize();
};

} // end namespace Builtins

#endif //CMATHICS_ARITHMETIC_H
