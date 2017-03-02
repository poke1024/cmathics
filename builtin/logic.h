#ifndef CMATHICS_LOGIC_H
#define CMATHICS_LOGIC_H

#include "../core/runtime.h"

namespace Builtins {

class Logic : public Unit {
public:
	Logic(Runtime &runtime) : Unit(runtime) {
	}

	void initialize();
};

}

#endif //CMATHICS_LOGIC_H
