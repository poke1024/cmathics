#ifndef CMATHICS_COMPARISON_H
#define CMATHICS_COMPARISON_H

#include "../core/runtime.h"

namespace Builtins {

    class Comparison : public Unit {
    public:
        Comparison(Runtime &runtime) : Unit(runtime) {
        }

        void initialize();
    };

} // end namespace Builtins

#endif //CMATHICS_COMPARISON_H
