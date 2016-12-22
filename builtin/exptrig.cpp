#include "exptrig.h"

#include "control.h"

template<typename F>
void Builtins::ExpTrig::add_unary(
    const char *name,
    const F& f) {

    add(name, Attributes::None, {
        builtin<1>([&f] (
            BaseExpressionPtr x,
            const Evaluation &evaluation) {

            if (x->type() == MachineRealType) {
                return Pool::MachineReal(f(
                    static_cast<const MachineReal*>(x)->value));
            } else {
                return BaseExpressionRef(); // leave to SymEngine
            }

        })
    });

}

void Builtins::ExpTrig::initialize() {

    add_unary("Sin", [] (machine_real_t x) {
        return std::sin(x); });
    add_unary("Cos", [] (machine_real_t x) {
        return std::cos(x); });
    add_unary("Tan", [] (machine_real_t x) {
        return std::tan(x); });
}
