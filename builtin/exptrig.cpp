// @formatter:off

#include "exptrig.h"
#include "control.h"

template<typename F>
class Unary : public Builtin {
private:
	const SymEngineUnaryFunction m_sym_engine_function;
    const F m_machine_function;

protected:
    template<typename... Args>
    Unary(SymEngineUnaryFunction sf, const F &f, Args&... args) :
		Builtin(args...), m_sym_engine_function(sf), m_machine_function(f) {
    }

public:
    static constexpr auto attributes =
        Attributes::Listable + Attributes::NumericFunction;

    void build(Runtime &runtime) {
        builtin(&Unary::apply);
    }

    inline BaseExpressionRef apply(
        ExpressionPtr expr,
        BaseExpressionPtr x,
        const Evaluation &evaluation) {

        if (x->is_machine_real()) {
            return MachineReal::construct(m_machine_function(
                static_cast<const MachineReal*>(x)->value));
        } else {
            return expr->symbolic_evaluate_unary(m_sym_engine_function, evaluation);
        }
    }
};

const auto _log = [] (machine_real_t x) {
    return std::log(x);
};

const auto _sin = [] (machine_real_t x) {
    return std::sin(x);
};

const auto _cos = [] (machine_real_t x) {
    return std::cos(x);
};

const auto _tan = [] (machine_real_t x) {
    return std::tan(x);
};

class Log : public Unary<decltype(_log)> {
public:
    static constexpr const char *name = "Log";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Log[$z$]'
        <dd>returns the natural logarithm of $z$.
    </dl>

    >> Log[{0, 1, E, E * E, E ^ 3, E ^ x}]
     = {-Infinity, 0, 1, 2, 3, Log[E ^ x]}
    >> Log[0.]
     = Indeterminate
    >> Plot[Log[x], {x, 0, 5}]
     = -Graphics-

    #> Log[1000] / Log[10] // Simplify
     = 3

    #> Log[1.4]
     = 0.336472

    #> Log[Exp[1.4]]
     = 1.4

    #> Log[-1.4]
     = 0.336472 + 3.14159 I

    #> N[Log[10], 30]
     = 2.30258509299404568401799145468
    )";

    template<typename... Args>
    Log(Args&... args) : Unary(SymEngine::log, _log, args...) {
    }

    void build(Runtime &runtime) {
        builtin("Log[0.]", "Indeterminate");
        builtin("Log[0]", "DirectedInfinity[-1]");
        builtin("Log[1]", "0");
        builtin("Log[E]", "1");
        builtin("Log[E^x_Integer]", "x");
        // builtin("Derivative[1][Log]", "1/#&");
        // builtin("Log[x_?InexactNumberQ]", "Log[E, x]");
        Unary<decltype(_log)>::build(runtime);
	    builtin(&Log::apply_2);
    }

	inline BaseExpressionRef apply_2(
		ExpressionPtr expr,
		BaseExpressionPtr,
		BaseExpressionPtr,
		const Evaluation &evaluation) {

		return expr->symbolic_evaluate_binary(
			SymEngine::log, evaluation);
	}
};

class Log2 : public Builtin {
private:
    UnsafeBaseExpressionRef m_log;
    UnsafeBaseExpressionRef m_two;

public:
    static constexpr const char *name = "Log2";

    static constexpr auto attributes =
        Attributes::Listable + Attributes::NumericFunction;

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Log2[$z$]'
        <dd>returns the base-2 logarithm of $z$.
    </dl>

    #> Log2[4 ^ 8]
     = 16
    >> Log2[5.6]
     = 2.48543
    >> Log2[E ^ 2]
     = 2 / Log[2]
    )";

    using Builtin::Builtin;

    void build(Runtime &runtime) {
        m_log = runtime.definitions().lookup("System`Log");
        m_two = MachineInteger::construct(2);

        builtin(&Log2::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr x,
        const Evaluation &evaluation) {

        if (x->is_machine_real()) {
            return MachineReal::construct(std::log2(
                static_cast<const MachineReal*>(x)->value));
        } else {
            return expression(m_log, x, m_two);
        }
    }
};

class Log10 : public Builtin {
private:
    UnsafeBaseExpressionRef m_log;
    UnsafeBaseExpressionRef m_ten;

public:
    static constexpr const char *name = "Log10";

    static constexpr auto attributes =
        Attributes::Listable + Attributes::NumericFunction;

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Log10[$z$]'
        <dd>returns the base-10 logarithm of $z$.
    </dl>

    #> Log10[1000]
     = 3
    >> Log10[{2., 5.}]
     = {0.30103, 0.69897}
    >> Log10[E ^ 3]
     = 3 / Log[10]
    )";

    using Builtin::Builtin;

    void build(Runtime &runtime) {
        m_log = runtime.definitions().lookup("System`Log");
        m_ten = MachineInteger::construct(10);

        builtin(&Log10::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr x,
        const Evaluation &evaluation) {

        if (x->is_machine_real()) {
            return MachineReal::construct(std::log10(
                static_cast<const MachineReal*>(x)->value));
        } else {
            return expression(m_log, x, m_ten);
        }
    }
};

class Sin : public Unary<decltype(_sin)> {
public:
    static constexpr const char *name = "Sin";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Sin[$z$]'
        <dd>returns the sine of $z$.
    </dl>

    >> Sin[0]
     = 0
    >> Sin[0.5]
     = 0.479426
    >> Sin[3 Pi]
     = 0
    >> Sin[1.0 + I]
     = 1.29846 + 0.634964 I

    >> Plot[Sin[x], {x, -Pi, Pi}]
     = -Graphics-

    >> N[Sin[1], 40]
     = 0.8414709848078965066525023216302989996226
    )";

    template<typename... Args>
    Sin(Args&... args) : Unary(SymEngine::sin, _sin, args...) {
    }
};

class Cos : public Unary<decltype(_cos)> {
public:
    static constexpr const char *name = "Cos";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Cos[$z$]'
        <dd>returns the cosine of $z$.
    </dl>

    >> Cos[3 Pi]
     = -1

    #> Cos[1.5 Pi]
     = -1.83697*^-16
    )";

    template<typename... Args>
    Cos(Args&... args) : Unary(SymEngine::cos, _cos, args...) {
    }
};

class Tan : public Unary<decltype(_tan)> {
public:
    static constexpr const char *name = "Tan";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Tan[$z$]'
        <dd>returns the tangent of $z$.
    </dl>

    >> Tan[0]
     = 0
    #> Tan[Pi / 2]
     = ComplexInfinity

    #> Tan[0.5 Pi]
     = 1.63312*^16
    )";

    template<typename... Args>
    Tan(Args&... args) : Unary(SymEngine::tan, _tan, args...) {
    }
};

void Builtins::ExpTrig::initialize() {
    add("Exp",
        Attributes::Listable + Attributes::NumericFunction, {
        down("Exp[x_]", "E ^ x")
        }
    );

    add<Log>();
    add<Log2>();
    add<Log10>();
    add<Sin>();
    add<Cos>();
    add<Tan>();
}
