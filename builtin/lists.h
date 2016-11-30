#ifndef CMATHICS_LIST_H
#define CMATHICS_LIST_H

#include "../core/runtime.h"

namespace Builtin {

class Lists : public Unit {
public:
	Lists(Runtime &runtime) : Unit(runtime) {
	}

	void initialize();
};

} // end namespace Builtin

#endif //CMATHICS_LIST_H
