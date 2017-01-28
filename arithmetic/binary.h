// @formatter:off

#ifndef CMATHICS_BINARY_H
#define CMATHICS_BINARY_H

struct no_binary_fallback {
    static inline BaseExpressionRef fallback(
        const BaseExpressionPtr head,
        const BaseExpressionPtr a,
        const BaseExpressionPtr b,
        const Evaluation &evaluation) {

        return BaseExpressionRef();
    }
};

template<typename F>
class BinaryOperator {
public:
    typedef std::function<BaseExpressionRef(
        const BaseExpressionPtr head,
        const BaseExpressionPtr a,
        const BaseExpressionPtr b,
        const Evaluation &evaluation)> Function;

protected:
    Function m_functions[1LL << (2 * CoreTypeBits)];

    template<typename U, typename V>
    void init(const Function &f) {
	    m_functions[size_t(U::Type) | (size_t(V::Type) << CoreTypeBits)] = f;
    }

    template<typename U, typename V>
    void clear() {
	    m_functions[size_t(U::Type) | (size_t(V::Type) << CoreTypeBits)] = F::fallback;
    }

    template<typename U, typename V>
    void init() {
        init<U, V>([] (
		    const BaseExpressionPtr,
		    const BaseExpressionPtr a,
		    const BaseExpressionPtr b,
		    const Evaluation &evaluation) -> BaseExpressionRef {

            return F::function(
		        *static_cast<const U*>(a),
		        *static_cast<const V*>(b),
	            evaluation);
        });
    };

public:
    inline BinaryOperator() {
	    for (size_t i = 0; i < sizeof(m_functions) / sizeof(Function); i++) {
		    m_functions[i] = F::fallback;
	    }
    }

    inline BaseExpressionRef operator()(
        const Definitions &definitions,
        const BaseExpressionPtr head,
        const BaseExpressionPtr a,
        const BaseExpressionPtr b,
        const Evaluation &evaluation) const {

        const Function &f = m_functions[a->type() | (size_t(b->type()) << CoreTypeBits)];
        return f(head, a, b, evaluation);
    }

    inline BaseExpressionRef operator()(
        const Definitions &definitions,
        const Expression *expr,
        const Evaluation &evaluation) const {

        const BaseExpressionRef * const leaves = expr->static_leaves<2>();

        const BaseExpression * const a = leaves[0].get();
        const BaseExpression * const b = leaves[1].get();

        return (*this)(definitions, expr->head(), a, b, evaluation);
    }
};

template<typename F>
class BinaryArithmetic : public BinaryOperator<F> {
public:
    using Base = BinaryOperator<F>;

    BinaryArithmetic(const Definitions &definitions) : Base() {

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
