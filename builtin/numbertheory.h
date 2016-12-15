#ifndef CMATHICS_NUMBERTHEORY_H
#define CMATHICS_NUMBERTHEORY_H

#include "../core/runtime.h"

namespace Builtins {

    class NumberTheory : public Unit {
    public:
        NumberTheory(Runtime &runtime) : Unit(runtime) {
        }

        void initialize();
    };

} // end namespace Builtins

#endif //CMATHICS_NUMBERTHEORY_H
