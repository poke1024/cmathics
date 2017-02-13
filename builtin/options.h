#ifndef CMATHICS_OPTIONS_H
#define CMATHICS_OPTIONS_H

#include "../core/runtime.h"

namespace Builtins {

    class Options : public Unit {
    public:
        Options(Runtime &runtime) : Unit(runtime) {
        }

        void initialize();
    };

} // end namespace Builtins

#endif //CMATHICS_OPTIONS_H
