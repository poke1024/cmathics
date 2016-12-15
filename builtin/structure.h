#ifndef CMATHICS_BUILTINS_STRUCTURE_H
#define CMATHICS_BUILTINS_STRUCTURE_H

#include "../core/runtime.h"

namespace Builtins {

    class Structure : public Unit {
    public:
        Structure(Runtime &runtime) : Unit(runtime) {
        }

        void initialize();
    };

} // end namespace Builtins

#endif //CMATHICS_BUILTINS_STRUCTURE_H
