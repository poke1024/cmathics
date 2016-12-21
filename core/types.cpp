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
			storage << expression(Blank);
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
			storage << expression(Blank);
		}
		storage << expression(BlankNullSequence);
	}, n + 1);
}

BaseExpressionRef function_pattern(
	const SymbolRef &head, const Definitions &definitions) {

	const auto &symbols = definitions.symbols();

	const auto &BlankSequence = symbols.BlankSequence;
	const auto &BlankNullSequence = symbols.BlankNullSequence;

	return expression(expression(head, expression(BlankSequence)), expression(BlankNullSequence));
}

MatchSize Rule::leaf_match_size() const {
	if (pattern->type() != ExpressionType) {
		return MatchSize::exactly(0); // undefined
	} else {
		return pattern->as_expression()->leaf_match_size();
	}
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
	    case BigRationalType:
		    return "BigRational";
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

/*std::ostream &operator<<(std::ostream &s, const Match &m) {
    s << "Match<" << (m ? "true" : "false"); // << ", ";
    auto node = m.variables();
    s << "{";
    if (node) {
        bool first = true;
        while (node) {
            const BaseExpressionRef &value = node->value;
            if (!first) {
                s << ", ";
            }
            s << node->variable->fullform() << ":" << value->fullform();
            first = false;
	        node = node->next_in_list;
        }
    }
    s << "}";
    s << ">";
    return s;
}*/

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
            return Heap::NoSymbolicForm();
        }
    });

	add(SymbolLog, [] (const Expression *expr) {
		if (expr->size() == 1) {
			return expr->symbolic_1(SymEngine::log);
		} else if (expr->size() == 2) {
			return expr->symbolic_2(SymEngine::log);
		} else {
			return Heap::NoSymbolicForm();
		}
	});

    add(SymbolCos, [] (const Expression *expr) {
        if (expr->size() == 1) {
            return expr->symbolic_1(SymEngine::cos);
        } else {
            return Heap::NoSymbolicForm();
        }
    });

    add(SymbolSin, [] (const Expression *expr) {
        if (expr->size() == 1) {
            return expr->symbolic_1(SymEngine::sin);
        } else {
            return Heap::NoSymbolicForm();
        }
    });

	add(SymbolTan, [] (const Expression *expr) {
		if (expr->size() == 1) {
			return expr->symbolic_1(SymEngine::tan);
		} else {
			return Heap::NoSymbolicForm();
		}
	});
}

BaseExpressionRef from_symbolic_expr(
	const SymEngineRef &ref,
	const BaseExpressionRef &head,
	const Evaluation &evaluation) {

	const auto &args = ref->get_args();
	return expression(
		head,
		[&args, &evaluation] (auto &storage) {
			for (const auto &arg : args) {
				storage << from_symbolic_form(arg, evaluation);
			}
		},
		args.size());
}

BaseExpressionRef from_symbolic_form(const SymEngineRef &form, const Evaluation &evaluation) {
    BaseExpressionRef expr;

	switch (form->get_type_code()) {
		case SymEngine::INTEGER: {
			const mpz_class value(static_cast<const SymEngine::Integer*>(form.get())->i.get_mpz_t());
			expr = from_primitive(value); // will instantiate a MachineInteger or a BigInteger
            break;
		}

		case SymEngine::REAL_DOUBLE: {
			const machine_real_t &value(static_cast<const SymEngine::RealDouble*>(form.get())->i);
			expr = Heap::MachineReal(value);
			break;
		}

		case SymEngine::REAL_MPFR: {
			// FIXME; same problem as in BigReal::instantiate_symbolic_form; we lose arb's radius

			const SymEngine::mpfr_class &value(static_cast<const SymEngine::RealMPFR*>(form.get())->i);

			arf_t temp;
			arf_init(temp);
			arf_set_mpfr(temp, value.get_mpfr_t());

			arb_t r;
			arb_init(r);
			arb_set_arf(r, temp);

			arf_clear(temp);

			expr = Heap::BigReal(r, Precision(value.get_prec()));
			break;
		}

		case SymEngine::RATIONAL: {
			const mpq_class value(static_cast<const SymEngine::Rational*>(form.get())->i.get_mpq_t());
			expr = Heap::BigRational(value);
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

		case SymEngine::LOG:
			expr = from_symbolic_expr(form, evaluation.Log, evaluation);
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

    expr->set_simplified_form(form);
    return expr;
}
