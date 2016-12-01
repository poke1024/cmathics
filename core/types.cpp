#include "types.h"
#include "expression.h"
#include "pattern.h"
#include "evaluate.h"

#include <symengine/cwrapper.h>

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

InstantiateSymbolicForm::Function InstantiateSymbolicForm::s_functions[256];

void InstantiateSymbolicForm::add(ExtendedType type, const Function &f) {
    s_functions[index(type)] = f;
}

void InstantiateSymbolicForm::init() {
    std::memset(s_functions, 0, sizeof(s_functions));

	add(SymbolPlus, [] (const Expression *expr) {
		if (expr->size() == 2) {
			return expr->symbolic_2(SymEngine::add);
		} else {
			return expr->symbolic_n(SymEngine::add);
		}
	});

    add(SymbolTimes, [] (const Expression *expr) {
        if (expr->size() == 2) {
            return expr->symbolic_2(SymEngine::mul);
        } else {
            return expr->symbolic_n(SymEngine::mul);
        }
    });

    add(SymbolPower, [] (const Expression *expr) {
        if (expr->size() == 2) {
            return expr->symbolic_2(SymEngine::pow);
        } else {
            return false;
        }
    });

    add(SymbolCos, [] (const Expression *expr) {
        if (expr->size() == 1) {
            return expr->symbolic_1(SymEngine::cos);
        } else {
            return false;
        }
    });

    add(SymbolSin, [] (const Expression *expr) {
        if (expr->size() == 1) {
            return expr->symbolic_1(SymEngine::sin);
        } else {
            return false;
        }
    });

	add(SymbolTan, [] (const Expression *expr) {
		if (expr->size() == 1) {
			return expr->symbolic_1(SymEngine::tan);
		} else {
			return false;
		}
	});
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
    BaseExpressionRef expr;

	switch (form->get_type_code()) {
		case SymEngine::INTEGER: {
			const mpz_class value(static_cast<const SymEngine::Integer*>(form.get())->i.get_mpz_t());
			expr = from_primitive(value);
            break;
		}

		case SymEngine::RATIONAL: {
			const mpq_class value(static_cast<const SymEngine::Rational*>(form.get())->i.get_mpq_t());
			expr = from_primitive(value);
            break;
		}

		case SymEngine::SYMBOL: {
			const std::string &name = static_cast<const SymEngine::Symbol*>(form.get())->get_name();
			const Symbol *addr;
			assert(name.length() == sizeof(addr) / sizeof(char));
			std::memcpy(&addr, name.data(), sizeof(addr));
			expr = BaseExpressionRef(addr);
            break;
		}

		case SymEngine::ADD:
			expr = from_symbolic_expr(form, evaluation.Plus, evaluation);
            break;

		case SymEngine::MUL:
			expr = from_symbolic_expr(form, evaluation.Times, evaluation);
            break;

		case SymEngine::POW:
			expr = from_symbolic_expr(form, evaluation.Power, evaluation);
            break;

        case SymEngine::COS:
            expr = from_symbolic_expr(form, evaluation.Cos, evaluation);
            break;

        case SymEngine::SIN:
            expr = from_symbolic_expr(form, evaluation.Sin, evaluation);
            break;

        case SymEngine::TAN:
            expr = from_symbolic_expr(form, evaluation.Tan, evaluation);
            break;

		case SymEngine::CONSTANT:
			if (form->__eq__(*SymEngine::pi.get())) {
				expr = evaluation.Pi;
                break;
			}
            else if (form->__eq__(*SymEngine::I.get())) {
                expr = evaluation.I;
                break;
            }
            else if (form->__eq__(*SymEngine::E.get())) {
                expr = evaluation.E;
                break;
            }
            else if (form->__eq__(*SymEngine::EulerGamma.get())) {
                expr = evaluation.EulerGamma;
                break;
            }
			// fallthrough

		default: {
			std::ostringstream s;
			s << "unexpected SymEngine type code " << form->get_type_code();
			throw std::runtime_error(s.str());
		}
	}

    expr->set_symbolic_form(form);
    return expr;
}
