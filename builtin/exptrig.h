#ifndef CMATHICS_EXPTRIG_H
#define CMATHICS_EXPTRIG_H

#include "../core/runtime.h"

namespace Builtins {

    class ExpTrig : public Unit {
    private:
        template<typename F>
        void add_unary(
            const char *name,
            const F& f);

    public:
        ExpTrig(Runtime &runtime) : Unit(runtime) {
        }

        void initialize();
    };

} // end namespace Builtins

#endif //CMATHICS_EXPTRIG_H
