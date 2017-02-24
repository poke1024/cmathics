#pragma once

#include "../core/runtime.h"

namespace Builtins {

    class Patterns : public Unit {
    public:
        Patterns(Runtime &runtime) : Unit(runtime) {
        }

        void initialize();
    };

} // end namespace Builtins
