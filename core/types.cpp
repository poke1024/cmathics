#include "types.h"
#include "expression.h"
#include "pattern.h"
#include "evaluate.h"

#include <symengine/cwrapper.h>

BaseExpressionRef exactly_n_pattern(
	const SymbolRef &head, size_t n, const Definitions &definitions) {

	const auto &Blank = definitions.symbols().Blank;
	return expression(head, sequential([n, &Blank] (auto &store) {
		for (size_t i = 0; i < n; i++) {
			store(expression(Blank));
		}
	}, n));
}

BaseExpressionRef at_least_n_pattern(
	const SymbolRef &head, size_t n, const Definitions &definitions) {

	const auto &symbols = definitions.symbols();
	const auto &Blank = symbols.Blank;
	const auto &BlankNullSequence = symbols.BlankNullSequence;

	return expression(head, sequential([n, &Blank, &BlankNullSequence] (auto &store) {
		for (size_t i = 0; i < n; i++) {
			store(expression(Blank));
		}
		store(expression(BlankNullSequence));
	}, n + 1));
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

bool BaseExpression::is_numeric() const {
	throw std::runtime_error("is_numeric not implemented");
}

SortKey BaseExpression::sort_key() const {
	return SortKey(0, 0); // FIXME
}

SortKey BaseExpression::pattern_key() const {
	return SortKey(0, 0, 1, 1, 0, 0, 0, 1);
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
	    case MachineComplexType:
		    return "MachineComplex";
        case BigComplexType:
            return "BigComplex";
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

inline SymbolicFormRef times_2(const Expression *expr) {
	const BaseExpressionRef * const leaves = expr->static_leaves<2>();

	for (int i = 0; i < 2; i++) {
		const BaseExpressionRef &operand = leaves[i];

		if (operand->type() == MachineIntegerType &&
		    static_cast<const MachineInteger*>(operand.get())->value == -1) {

			const SymbolicFormRef form = unsafe_symbolic_form(leaves[1 - i]);
			if (!form->is_none()) {
				return Pool::SymbolicForm(SymEngine::neg(form->get()));
			} else {
				return Pool::NoSymbolicForm();
			}
		}
	}

	return expr->symbolic_2(SymEngine::mul);
}

void InstantiateSymbolicForm::init() {
    std::memset(s_functions, 0, sizeof(s_functions));

	add(SymbolDirectedInfinity, [] (const Expression *expr) {
		if (expr->size() == 1) {
			const BaseExpressionRef &leaf = expr->static_leaves<1>()[0];
			if (leaf->type() == MachineIntegerType) {
				const machine_integer_t direction = static_cast<const MachineInteger*>(leaf.get())->value;
				if (direction > 0) {
					return SymbolicFormRef(Pool::SymbolicForm(SymEngineRef(SymEngine::Inf.get()), true));
				} else if (direction < 0) {
					return SymbolicFormRef(Pool::SymbolicForm(SymEngineRef(SymEngine::NegInf.get()), true));
				}
			}
		}

		return Pool::NoSymbolicForm();
	});

	add(SymbolPlus, [] (const Expression *expr) {
		if (expr->size() == 2) {
			return expr->symbolic_2(SymEngine::add);
		} else {
			return expr->symbolic_n(SymEngine::add);
		}
	});

    add(SymbolTimes, [] (const Expression *expr) {
        if (expr->size() == 2) {
            return times_2(expr);
        } else {
            return expr->symbolic_n(SymEngine::mul);
        }
    });

    add(SymbolPower, [] (const Expression *expr) {
        if (expr->size() == 2) {
            return expr->symbolic_2(SymEngine::pow);
        } else {
            return Pool::NoSymbolicForm();
        }
    });

	add(SymbolLog, [] (const Expression *expr) {
		if (expr->size() == 1) {
			return expr->symbolic_1(SymEngine::log);
		} else if (expr->size() == 2) {
			return expr->symbolic_2(SymEngine::log);
		} else {
			return Pool::NoSymbolicForm();
		}
	});

    add(SymbolCos, [] (const Expression *expr) {
        if (expr->size() == 1) {
            return expr->symbolic_1(SymEngine::cos);
        } else {
            return Pool::NoSymbolicForm();
        }
    });

    add(SymbolSin, [] (const Expression *expr) {
        if (expr->size() == 1) {
            return expr->symbolic_1(SymEngine::sin);
        } else {
            return Pool::NoSymbolicForm();
        }
    });

	add(SymbolTan, [] (const Expression *expr) {
		if (expr->size() == 1) {
			return expr->symbolic_1(SymEngine::tan);
		} else {
			return Pool::NoSymbolicForm();
		}
	});

    /*add(SymbolFactorial, [] (const Expression *expr) {
        if (expr->size() == 1) {
            // currently always expects SymEngine::Integer
            return expr->symbolic_1(SymEngine::factorial);
        } else {
            return Pool::NoSymbolicForm();
        }
    });*/

    add(SymbolGamma, [] (const Expression *expr) {
        if (expr->size() == 1) {
            return expr->symbolic_1(SymEngine::gamma);
        } else if (expr->size() == 2) {
            return expr->symbolic_2(SymEngine::uppergamma);
        } else {
            return Pool::NoSymbolicForm();
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
#if 1
		sequential([&args, &evaluation] (auto &store) {
			for (size_t i = 0; i < args.size(); i++) {
				store(from_symbolic_form(args[i], evaluation));
			}
		},
#else
		parallel([&args, &evaluation] (size_t i) {
			// FIXME ensure that arg[i] can be accessed concurrently
			return from_symbolic_form(args[i], evaluation);
		},
#endif
		args.size()));
}

BaseExpressionRef from_symbolic_expr_with_sort(
	const SymEngineRef &ref,
	const BaseExpressionRef &head,
	const Evaluation &evaluation) {

	const auto &args = ref->get_args();

	std::vector<UnsafeBaseExpressionRef> sorted_args;
	sorted_args.reserve(args.size());
	for (size_t i = 0; i < args.size(); i++) {
		sorted_args.emplace_back(from_symbolic_form(args[i], evaluation));
	}

	std::sort(
		sorted_args.begin(),
		sorted_args.end(),
		[] (const BaseExpressionRef &x, const BaseExpressionRef &y) {
			return x->sort_key().compare(y->sort_key()) < 0;
		});

	return expression(head, LeafVector(std::move(sorted_args)));
}

BaseExpressionRef from_symbolic_form(const SymEngineRef &form, const Evaluation &evaluation) {
    UnsafeBaseExpressionRef expr;

	switch (form->get_type_code()) {
		case SymEngine::INTEGER: {
			const mpz_class value(static_cast<const SymEngine::Integer*>(form.get())->i.get_mpz_t());
			expr = from_primitive(value); // will instantiate a MachineInteger or a BigInteger
            break;
		}

		case SymEngine::REAL_DOUBLE: {
			const machine_real_t &value(static_cast<const SymEngine::RealDouble*>(form.get())->i);
			expr = Pool::MachineReal(value);
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

			expr = Pool::BigReal(r, Precision(value.get_prec()));
			break;
		}

		case SymEngine::RATIONAL: {
			const mpq_class value(static_cast<const SymEngine::Rational*>(form.get())->i.get_mpq_t());
			expr = Pool::BigRational(value);
            break;
		}

        case SymEngine::COMPLEX:
            expr = Pool::BigComplex(SymEngineComplexRef(static_cast<const SymEngine::Complex*>(form.get())));
            break;

        case SymEngine::COMPLEX_DOUBLE: {
            const auto *complex = static_cast<const SymEngine::ComplexDouble*>(form.get());
            expr = Pool::MachineComplex(complex->i.real(), complex->i.imag());
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
			expr = from_symbolic_expr_with_sort(form, evaluation.Plus, evaluation);
            break;

		case SymEngine::MUL:
			expr = from_symbolic_expr_with_sort(form, evaluation.Times, evaluation);
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

        case SymEngine::GAMMA:
        case SymEngine::LOWERGAMMA:
        case SymEngine::UPPERGAMMA:
            expr = from_symbolic_expr(form, evaluation.Gamma, evaluation);
            break;

		case SymEngine::INFTY: {
			const auto * const infty = static_cast<const SymEngine::Infty*>(form.get());
			if (infty->is_positive()) {
				expr = expression(evaluation.DirectedInfinity, Pool::MachineInteger(1));
			} else if (infty->is_negative()) {
				expr = expression(evaluation.DirectedInfinity, Pool::MachineInteger(-1));
			} else {
				throw std::runtime_error("cannot handle unsigned infinity from SymEngine");
			}
			break;
		}

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
			else if (form->__eq__(*SymEngine::Inf.get())) {
				expr = expression(evaluation.DirectedInfinity, Pool::MachineInteger(1));
				break;
			}
			else if (form->__eq__(*SymEngine::NegInf.get())) {
				expr = expression(evaluation.DirectedInfinity, Pool::MachineInteger(-1));
				break;
			}
			// fallthrough

		default: {
			std::ostringstream s;
			s << "unsupported SymEngine type code " << form->get_type_code();
			throw std::runtime_error(s.str());
		}
	}

    expr->set_simplified_form(form);
    return expr;
}
