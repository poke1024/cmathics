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

class Quotient : public Builtin {
public:
    static constexpr const char *name = "Quotient";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Quotient[m, n]'
      <dd>computes the integer quotient of $m$ and $n$.
    </dl>

    >> Quotient[23, 7]
     = 3

    >> Quotient[13, 0]
     : Infinite expression Quotient[13, 0] encountered.
     = ComplexInfinity
    >> Quotient[-17, 7]
     = -3
    >> Quotient[-17, -4]
     = 4
    >> Quotient[19, -4]
     = -5
	)";

    static constexpr auto attributes =
        Attributes::Listable + Attributes::NumericFunction;

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        message("infy", "Infinite expression `1` encountered.");

        builtin("Quotient[m_Integer, n_Integer]", &Quotient::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr m,
        BaseExpressionPtr n,
        const Evaluation &evaluation) {

        const auto numeric_m = m->get_int_value();
        const auto numeric_n = n->get_int_value();
        assert(numeric_m && numeric_n);

        if ((*numeric_n).is_zero()) {
            evaluation.message(m_symbol, "infy", expression(m_symbol, m, n));
            return evaluation.ComplexInfinity;
        }

        return (*numeric_m / *numeric_n).to_expression();
    }
};

void Builtins::NumberTheory::initialize() {
    add<EvenQ>();
    add<OddQ>();
    add<Quotient>();
}
