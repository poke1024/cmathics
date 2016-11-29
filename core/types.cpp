#include "types.h"
#include "expression.h"
#include "pattern.h"
#include "evaluate.h"

BaseExpressionRef exactly_n_pattern(
	const SymbolRef &head, size_t n, const Definitions &definitions) {

	const auto &Blank = definitions.symbols().Blank;
	return expression(head, [n, &Blank] (auto &storage) {
		for (size_t i = 0; i < n; i++) {
			storage << Blank;
		}
	}, n);
}

BaseExpressionRef at_least_n_pattern(
	const SymbolRef &head, size_t n, const Definitions &definitions) {

	const auto &symbols = definitions.symbols();
	const auto &Blank = symbols.Blank;
	const auto &BlankNullSequence = symbols.BlankNullSequence;

	return expression(head, [n, &Blank, &BlankNullSequence] (auto &storage) {
		for (size_t i = 0; i < n; i++) {
			storage << Blank;
		}
		storage << BlankNullSequence;
	}, n + 1);
}

BaseExpressionRef function_pattern(
	const SymbolRef &head, const Definitions &definitions) {

	const auto &symbols = definitions.symbols();

	const auto &BlankSequence = symbols.BlankSequence;
	const auto &BlankNullSequence = symbols.BlankNullSequence;

	return expression(expression(head, {BlankSequence}), {BlankNullSequence});
}

MatchSize Rule::match_size() const {
	return pattern->match_size();
}

SortKey BaseExpression::sort_key() const {
	throw std::runtime_error("not implemented");
}

SortKey BaseExpression::pattern_key() const {
	return SortKey(0, 0, 1, 1, 0, nullptr, 1);
}

const char *type_name(Type type) {
    switch (type) {
	    case MachineIntegerType:
		    return "MachineInteger";
	    case BigIntegerType:
		    return "BigInteger";
	    case MachineRealType:
		    return "MachineReal";
	    case BigRealType:
		    return "BigReal";
	    case RationalType:
		    return "Rational";
	    case ComplexType:
		    return "Complex";
	    case ExpressionType:
		    return "Expression";
	    case SymbolType:
		    return "Symbol";
	    case StringType:
		    return "String";
	    default:
		    return "Unknown";
    }
}

std::ostream &operator<<(std::ostream &s, const Match &m) {
    s << "Match<" << (m ? "true" : "false"); // << ", ";
    auto var = m.variables();
    s << "{";
    if (var) {
        bool first = true;
        while (var) {
            BaseExpressionRef value = var->matched_value();
            if (!first) {
                s << ", ";
            }
            s << var->fullform() << ":" << value->fullform();
            first = false;
            var = var->next_variable();
        }
    }
    s << "}";
    s << ">";
    return s;
}

BaseExpressionRef from_symbolic_expr(
	const SymbolicForm &form,
	const BaseExpressionRef &head,
	const Evaluation &evaluation) {

	const auto &args = form->get_args();
	return expression(
		head,
		[&args, &evaluation] (auto &storage) {
			for (const auto &arg : args) {
				storage << from_symbolic_form(arg, evaluation);
			}
		},
		args.size());
}

BaseExpressionRef from_symbolic_form(const SymbolicForm &form, const Evaluation &evaluation) {
	switch (form->get_type_code()) {
		case SymEngine::INTEGER: {
			const mpz_class value(static_cast<const SymEngine::Integer*>(form.get())->i.get_mpz_t());
			return from_primitive(value);
		}

		case SymEngine::SYMBOL: {
			const std::string &name = static_cast<const SymEngine::Symbol*>(form.get())->get_name();
			const Symbol *addr;
			assert(name.length() == sizeof(addr) / sizeof(char));
			std::memcpy(&addr, name.data(), sizeof(addr));
			return BaseExpressionRef(addr);
		}

		case SymEngine::ADD:
			return from_symbolic_expr(form, evaluation.Plus, evaluation);

		case SymEngine::MUL:
			return from_symbolic_expr(form, evaluation.Times, evaluation);

		case SymEngine::POW:
			return from_symbolic_expr(form, evaluation.Power, evaluation);

		default:
			throw std::runtime_error("not implemented");
	}
}