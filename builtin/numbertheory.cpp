#include "numbertheory.h"

class EvenQ : public Builtin {
public:
    static constexpr const char *name = "EvenQ";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&EvenQ::test);
    }

    inline bool test(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        switch (expr->type()) {
            case MachineIntegerType:
                return (static_cast<const MachineInteger*>(expr)->value & 1) == 0;

            case BigIntegerType:
                return (static_cast<const BigInteger*>(expr)->value & 1) == 0;

            default:
                return false;
        }
    }
};

class OddQ : public Builtin {
public:
    static constexpr const char *name = "OddQ";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&OddQ::test);
    }

    inline bool test(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        switch (expr->type()) {
            case MachineIntegerType:
                return (static_cast<const MachineInteger*>(expr)->value & 1) != 0;

            case BigIntegerType:
                return (static_cast<const BigInteger*>(expr)->value & 1) != 0;

            default:
                return false;
        }
    }
};

void Builtins::NumberTheory::initialize() {
    add<EvenQ>();
    add<OddQ>();
}
