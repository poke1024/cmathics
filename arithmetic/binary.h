#pragma once
// @formatter:off

#ifndef CMATHICS_BINARY_H
#define CMATHICS_BINARY_H

#include "../core/runtime.h"

struct no_binary_fallback {
    static inline BaseExpressionRef fallback(
	    const ExpressionPtr expr,
        const Evaluation &evaluation) {

        return BaseExpressionRef();
    }
};

using BinaryArithmeticFunction = std::function<BaseExpressionRef(
	const ExpressionPtr expr,
	const Evaluation &evaluation)>;

using BinaryComparisonFunction = std::function<tribool(
	const BaseExpressionPtr a,
	const BaseExpressionPtr b,
	const Evaluation &evaluation)>;

using BinaryOrderFunction = std::function<long(
	const BaseExpressionPtr a,
	const BaseExpressionPtr b,
	const Evaluation &evaluation)>;

template<typename Function>
struct BinaryFunctionBridge {
};

template<>
struct BinaryFunctionBridge<BinaryArithmeticFunction> {
	template<typename F, typename U, typename V>
	static BinaryArithmeticFunction construct() {
		return [] (
			const ExpressionPtr expr,
			const Evaluation &evaluation) -> BaseExpressionRef {

			const BaseExpressionRef * const leaves = expr->n_leaves<2>();

			const BaseExpression * const a = leaves[0].get();
			const BaseExpression * const b = leaves[1].get();

			return F::function(
				*static_cast<const U*>(a),
				*static_cast<const V*>(b),
				evaluation);
		};
	}

	static inline BaseExpressionRef call(
		const BinaryArithmeticFunction *functions,
		const ExpressionPtr expr,
		const Evaluation &evaluation) {

		const BaseExpressionRef * const leaves = expr->n_leaves<2>();

		const BaseExpression * const a = leaves[0].get();
		const BaseExpression * const b = leaves[1].get();

		const BinaryArithmeticFunction &f =
			functions[a->type() | (size_t(b->type()) << CoreTypeBits)];
		return f(expr, evaluation);
	}
};

template<>
struct BinaryFunctionBridge<BinaryComparisonFunction> {
	template<typename F, typename U, typename V>
	static BinaryComparisonFunction construct() {
		return [] (
			const BaseExpressionPtr a,
			const BaseExpressionPtr b,
			const Evaluation &evaluation) -> tribool {

			return F::function(
				*static_cast<const U*>(a),
				*static_cast<const V*>(b),
				evaluation);
		};
	}

	static inline tribool call(
		const BinaryComparisonFunction *functions,
		const BaseExpressionPtr a,
		const BaseExpressionPtr b,
		const Evaluation &evaluation) {

		const BinaryComparisonFunction &f =
			functions[a->type() | (size_t(b->type()) << CoreTypeBits)];
		return f(a, b, evaluation);
	}
};

template<>
struct BinaryFunctionBridge<BinaryOrderFunction> {
	template<typename F, typename U, typename V>
	static BinaryOrderFunction construct() {
		return [] (
			const BaseExpressionPtr a,
			const BaseExpressionPtr b,
			const Evaluation &evaluation) -> long {

			return F::function(
				*static_cast<const U*>(a),
				*static_cast<const V*>(b),
				evaluation);
		};
	}

	static inline auto call(
		const BinaryOrderFunction *functions,
		const BaseExpressionPtr a,
		const BaseExpressionPtr b,
		const Evaluation &evaluation) {

		const BinaryComparisonFunction &f =
			functions[a->type() | (size_t(b->type()) << CoreTypeBits)];
		return f(a, b, evaluation);
	}
};

template<typename F>
class BinaryOperator {
public:
	using Function = typename F::Function;

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
        init<U, V>(BinaryFunctionBridge<Function>::template construct<F, U, V>());
    };

public:
    inline BinaryOperator(const Definitions &definitions) {
	    for (size_t i = 0; i < sizeof(m_functions) / sizeof(Function); i++) {
		    m_functions[i] = F::fallback;
	    }

	    this->template init<MachineInteger, MachineInteger>();
	    this->template init<MachineInteger, BigInteger>();
	    this->template init<MachineInteger, BigRational>();
	    this->template init<MachineInteger, MachineReal>();
	    this->template init<MachineInteger, BigReal>();

	    this->template init<BigInteger, MachineInteger>();
	    this->template init<BigInteger, BigInteger>();
	    this->template init<BigInteger, BigRational>();
	    this->template init<BigInteger, MachineReal>();
	    this->template init<BigInteger, BigReal>();

	    this->template init<BigRational, MachineInteger>();
	    this->template init<BigRational, BigInteger>();
	    this->template init<BigRational, MachineReal>();
	    this->template init<BigRational, BigReal>();
	    this->template init<BigRational, BigRational>();

	    this->template init<MachineReal, MachineInteger>();
	    this->template init<MachineReal, BigInteger>();
	    this->template init<MachineReal, BigRational>();
	    this->template init<MachineReal, MachineReal>();
	    this->template init<MachineReal, BigReal>();

	    this->template init<BigReal, MachineInteger>();
	    this->template init<BigReal, BigInteger>();
	    this->template init<BigReal, BigRational>();
	    this->template init<BigReal, MachineReal>();
	    this->template init<BigReal, BigReal>();
    }

	template<typename... Args>
	inline auto operator()(Args&&... args) const {
		return BinaryFunctionBridge<Function>::call(m_functions, std::forward<Args>(args)...);
	}
};

template<machine_integer_t Value>
class EmptyConstantRule :
    public ExactlyNRule<0>,
    public ExtendedHeapObject<EmptyConstantRule<Value>> {

private:
    const BaseExpressionRef m_value;

public:
    EmptyConstantRule(const SymbolRef &head, const Evaluation &evaluation) :
        ExactlyNRule<0>(head, evaluation),
        m_value(MachineInteger::construct(Value)) {
    }

    virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

        return m_value;
    }
};

class IdentityRule :
    public ExactlyNRule<1>,
    public ExtendedHeapObject<IdentityRule> {

public:
    IdentityRule(const SymbolRef &head, const Evaluation &evaluation) :
        ExactlyNRule<1>(head, evaluation) {
    }

    virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

        return expr->n_leaves<1>()[0];
    }
};

template<typename Operator>
class BinaryArithmeticRule :
    public ExactlyNRule<2>,
    public ExtendedHeapObject<BinaryArithmeticRule<Operator>> {

private:
    const Operator m_operator;

public:
	BinaryArithmeticRule(
        const SymbolRef &head,
        const Evaluation &evaluation) :

        ExactlyNRule<2>(head, evaluation), m_operator(evaluation.definitions) {
    }

    virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

        return m_operator(expr, evaluation);
    }
};

template<typename Operator>
class BinaryComparisonRule :
	public ExactlyNRule<2>,
	public ExtendedHeapObject<BinaryComparisonRule<Operator>> {

private:
	const Operator m_operator;

public:
	BinaryComparisonRule(
		const SymbolRef &head,
		const Evaluation &evaluation) :

		ExactlyNRule<2>(head, evaluation), m_operator(evaluation.definitions) {
	}

	virtual optional<BaseExpressionRef> try_apply(
		const Expression *expr,
		const Evaluation &evaluation) const {

		const BaseExpressionRef * const leaves = expr->n_leaves<2>();

		const BaseExpression * const a = leaves[0].get();
		const BaseExpression * const b = leaves[1].get();

		switch (m_operator(a, b, evaluation)) {
			case true:
				return BaseExpressionRef(evaluation.True);
			case false:
				return BaseExpressionRef(evaluation.False);
			case undecided:
				return BaseExpressionRef();
			default:
				throw std::runtime_error("illegal comparison result");
		}
	}
};

class BinaryOperatorBuiltin : public Builtin {
protected:
    void add_binary_operator_formats() {
	    std::string op_pattern;
	    const char *replace_items;

	    const char *group = grouping();

	    if (strcmp(group, "None") == 0 || strcmp(group, "NonAssociative") == 0) {
		    op_pattern = string_format("Verbatim[%s][items__]", m_symbol->name());
		    replace_items = "items";
	    } else {
		    op_pattern = string_format("Verbatim[%s][x_, y_]", m_symbol->name());
		    replace_items = "x, y";
	    }

        builtin(
            m_runtime.parse(
                "MakeBoxes[%s, form:StandardForm|TraditionalForm]",
		        op_pattern.c_str()),
            m_runtime.parse(
                "MakeBoxes[Infix[{%s}, \"%s\", %d, %s], form]",
		        replace_items,
                operator_name(),
                precedence(),
                group)
        );

        builtin(
            m_runtime.parse(
                "MakeBoxes[%s, form:InputForm|OutputForm]",
		        op_pattern.c_str()),
            m_runtime.parse(
                "MakeBoxes[Infix[{%s}, \" %s \", %d, %s], form]",
	            replace_items,
                operator_name(),
                precedence(),
                group)
        );
    }

public:
    using Builtin::Builtin;

    virtual const char *operator_name() const = 0;

    virtual int precedence() const = 0;

    virtual const char *grouping() const {
	    return "None";
    }
};

#endif //CMATHICS_BINARY_H
