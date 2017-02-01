#ifndef CMATHICS_EVALUATION_H
#define CMATHICS_EVALUATION_H

#include "../core/runtime.h"

namespace Builtins {

    class Evaluation : public Unit {
    public:
        Evaluation(Runtime &runtime) : Unit(runtime) {
        }

        void initialize();
    };

} // end namespace Builtins

#endif //CMATHICS_EVALUATION_H
