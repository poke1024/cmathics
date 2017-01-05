#include "exptrig.h"

#include "control.h"

template<typename F>
class Unary : public Builtin {
private:
    const F m_function;

protected:
    template<typename... Args>
    Unary(const F &f, Args&... args) : Builtin(args...), m_function(f) {
    }

public:
    void build(Runtime &runtime) {
        builtin(&Unary::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr x,
        const Evaluation &evaluation) {

        if (x->type() == MachineRealType) {
            return Pool::MachineReal(m_function(
                static_cast<const MachineReal*>(x)->value));
        } else {
            return BaseExpressionRef(); // leave to SymEngine
        }
    }
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
    Sin(Args&... args) : Unary(_sin, args...) {

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
    Cos(Args&... args) : Unary(_cos, args...) {
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
    Tan(Args&... args) : Unary(_tan, args...) {
    }
};

void Builtins::ExpTrig::initialize() {
    add<Sin>();
    add<Cos>();
    add<Tan>();
}
