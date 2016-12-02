#ifndef CMATHICS_LIST_H
#define CMATHICS_LIST_H

#include "../core/runtime.h"

namespace Builtins {

class Lists : public Unit {
public:
	Lists(Runtime &runtime) : Unit(runtime) {
	}

	void initialize();
};

} // end namespace Builtins

#endif //CMATHICS_LIST_H
