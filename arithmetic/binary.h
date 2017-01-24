// @formatter:off

#ifndef CMATHICS_BINARY_H
#define CMATHICS_BINARY_H

class no_binary_fallback {
public:
    inline BaseExpressionRef operator()(const Expression*, const Evaluation&) const {
        return BaseExpressionRef();
    }
};

template<typename F, typename Fallback = no_binary_fallback>
class BinaryOperator {
private:
    const Fallback m_fallback;

protected:
    typedef decltype(F::calculate(nullptr, nullptr)) IntermediateType;

    typedef std::function<IntermediateType(const BaseExpression *a, const BaseExpression *b)> Function;

    Function _functions[1LL << (2 * CoreTypeBits)];

    template<typename U, typename V>
    void init(const Function &f) {
        _functions[size_t(U::Type) | (size_t(V::Type) << CoreTypeBits)] = f;
    }

    template<typename U, typename V>
    void clear() {
        _functions[size_t(U::Type) | (size_t(V::Type) << CoreTypeBits)] = nullptr;
    }

    template<typename U, typename V>
    void init() {
        init<U, V>([] (const BaseExpression *a, const BaseExpression *b) {
            return F::calculate(*static_cast<const U*>(a), *static_cast<const V*>(b));
        });
    };

    static inline BaseExpressionRef result(const Definitions &definitions, const BaseExpressionRef &result) {
        return result;
    }

    static inline BaseExpressionRef result(const Definitions &definitions, bool result) {
        return definitions.symbols().Boolean(result);
    }

public:
    BinaryOperator(const Fallback &fallback = Fallback()) : m_fallback(fallback) {
        std::memset(_functions, 0, sizeof(_functions));
    }

    inline BaseExpressionRef operator()(
        const Definitions &definitions,
        const Expression *expr,
        const Evaluation &evaluation) const {

        const BaseExpressionRef * const leaves = expr->static_leaves<2>();

        const BaseExpression * const a = leaves[0].get();
        const BaseExpression * const b = leaves[1].get();
        const Function f = _functions[a->type() | (size_t(b->type()) << CoreTypeBits)];

        if (f) {
            return result(definitions, f(a, b));
        } else {
            return m_fallback(expr, evaluation);
        }
    }
};

template<typename F, typename Fallback = no_binary_fallback>
class BinaryArithmetic : public BinaryOperator<F, Fallback> {
public:
    using Base = BinaryOperator<F, Fallback>;

    BinaryArithmetic(const Definitions &definitions, const Fallback &fallback = Fallback()) : Base(fallback) {

        Base::template init<MachineInteger, MachineInteger>();
        Base::template init<MachineInteger, BigInteger>();
        Base::template init<MachineInteger, BigRational>();
        Base::template init<MachineInteger, MachineReal>();
        Base::template init<MachineInteger, BigReal>();

        Base::template init<BigInteger, MachineInteger>();
        Base::template init<BigInteger, BigInteger>();
        Base::template init<BigInteger, BigRational>();
        Base::template init<BigInteger, MachineReal>();
        Base::template init<BigInteger, BigReal>();

        Base::template init<BigRational, MachineInteger>();
        Base::template init<BigRational, BigInteger>();
        Base::template init<BigRational, MachineReal>();
        Base::template init<BigRational, BigReal>();
        Base::template init<BigRational, BigRational>();

        Base::template init<MachineReal, MachineInteger>();
        Base::template init<MachineReal, BigInteger>();
        Base::template init<MachineReal, BigRational>();
        Base::template init<MachineReal, MachineReal>();
        Base::template init<MachineReal, BigReal>();

        Base::template init<BigReal, MachineInteger>();
        Base::template init<BigReal, BigInteger>();
        Base::template init<BigReal, BigRational>();
        Base::template init<BigReal, MachineReal>();
        Base::template init<BigReal, BigReal>();
    }
};

template<machine_integer_t Value>
class EmptyConstantRule : public ExactlyNRule<0> {
private:
    const BaseExpressionRef m_value;

public:
    EmptyConstantRule(const SymbolRef &head, const Definitions &definitions) :
        ExactlyNRule<0>(head, definitions),
        m_value(Pool::MachineInteger(Value)) {
    }

    virtual BaseExpressionRef try_apply(
        const Expression *expr, const Evaluation &evaluation) const {

        return m_value;
    }
};

class IdentityRule : public ExactlyNRule<1> {
public:
    IdentityRule(const SymbolRef &head, const Definitions &definitions) :
        ExactlyNRule<1>(head, definitions) {
    }

    virtual BaseExpressionRef try_apply(
        const Expression *expr, const Evaluation &evaluation) const {

        return expr->static_leaves<1>()[0];
    }
};

template<typename Operator>
class BinaryOperatorRule : public ExactlyNRule<2> {
private:
    const Operator m_operator;

public:
    BinaryOperatorRule(
        const SymbolRef &head,
        const Definitions &definitions) :

        ExactlyNRule<2>(head, definitions), m_operator(definitions) {
    }

    virtual BaseExpressionRef try_apply(
        const Expression *expr, const Evaluation &evaluation) const {

        return m_operator(evaluation.definitions, expr, evaluation);
    }
};

#endif //CMATHICS_BINARY_H
