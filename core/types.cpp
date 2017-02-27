#include "types.h"
#include "core/expression/implementation.h"
#include "core/pattern/arguments.h"
#include "evaluate.h"

#include <symengine/cwrapper.h>

BaseExpressionRef BaseExpression::format(const BaseExpressionRef &form, const Evaluation &evaluation) const {
	BaseExpressionRef expr = coalesce(custom_format(form, evaluation), BaseExpressionRef(this));
	return expression(evaluation.MakeBoxes, expr, form)->evaluate_or_copy(evaluation);
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

BaseExpressionRef from_symbolic_expr_canonical(
	const SymEngineRef &ref,
	const BaseExpressionRef &head,
	const Evaluation &evaluation) {

	const auto &args = ref->get_args();

	LeafVector conv_args;
	conv_args.reserve(args.size());
	for (size_t i = 0; i < args.size(); i++) {
		conv_args.push_back(from_symbolic_form(args[i], evaluation));
	}
	conv_args.sort();

	return expression(head, std::move(conv_args));
}

BaseExpressionRef from_symbolic_form(const SymEngineRef &form, const Evaluation &evaluation) {
    UnsafeBaseExpressionRef expr;

	switch (form->get_type_code()) {
		case SymEngine::INTEGER: {
			const mpz_class value(static_cast<const SymEngine::Integer*>(form.get())->as_integer_class().get_mpz_t());
			expr = from_primitive(value); // will instantiate a MachineInteger or a BigInteger
            break;
		}

		case SymEngine::REAL_DOUBLE: {
			const machine_real_t &value(static_cast<const SymEngine::RealDouble*>(form.get())->i);
			expr = Pool::MachineReal(value);
			break;
		}

		case SymEngine::REAL_MPFR: {
			const SymEngine::mpfr_class &value(static_cast<const SymEngine::RealMPFR*>(form.get())->i);
			mpfr_t copy;
			mpfr_init2(copy, mpfr_get_prec(value.get_mpfr_t()));
			mpfr_set(copy, value.get_mpfr_t(), MPFR_RNDN);
			expr = Pool::BigReal(copy, Precision(value.get_prec()));
			break;
		}

		case SymEngine::RATIONAL: {
			const mpq_class value(static_cast<const SymEngine::Rational*>(form.get())->as_rational_class().get_mpq_t());
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
#if DEBUG_SYMBOLIC
			expr = BaseExpressionRef(evaluation.definitions.lookup(name.c_str()));
#else
			const Symbol *addr;
			assert(name.length() == sizeof(addr) / sizeof(char));
			std::memcpy(&addr, name.data(), sizeof(addr));
			expr = BaseExpressionRef(addr);
#endif
            break;
		}

		case SymEngine::ADD:
			expr = from_symbolic_expr_canonical(form, evaluation.Plus, evaluation);
            break;

		case SymEngine::MUL:
			expr = from_symbolic_expr_canonical(form, evaluation.Times, evaluation);
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

		case SymEngine::ABS:
			expr = from_symbolic_expr(form, evaluation.Abs, evaluation);
			break;

		case SymEngine::INFTY: {
			const auto * const infty = static_cast<const SymEngine::Infty*>(form.get());
			if (infty->is_positive()) {
				expr = expression(evaluation.DirectedInfinity, Pool::MachineInteger(1));
			} else if (infty->is_negative()) {
				expr = expression(evaluation.DirectedInfinity, Pool::MachineInteger(-1));
			} else if (infty->is_complex()) {
				expr = evaluation.ComplexInfinity;
			} else {
				throw std::runtime_error("cannot handle infinity from SymEngine");
			}
			break;
		}

		case SymEngine::NOT_A_NUMBER:
			expr = evaluation.Indeterminate;
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

    expr->set_symbolic_form(form);
    return expr;
}

Precision precision(const BaseExpressionRef &item) {
    switch (item->type()) {
        case MachineRealType:
            return Precision(mp_prec_t(std::numeric_limits<machine_real_t>::digits));

        case BigRealType:
            return static_cast<const BigReal*>(item.get())->prec;

        case MachineIntegerType:
            return Precision(mp_prec_t(std::numeric_limits<machine_integer_t>::digits));

        case BigIntegerType:
            return Precision(mp_prec_t(static_cast<const BigInteger*>(item.get())->value.get_prec()));

        case ExpressionType: {
            const Expression * const expr = static_cast<const Expression*>(item.get());

            return expr->with_slice([] (const auto &slice) {
                bool first_big = false;
                mp_prec_t bits = 0;

                const size_t n = slice.size();

                for (size_t i = 0; i < n; i++) {
                    const Precision r = precision(slice[i]);

                    if (r.is_machine_precision()) {
                        return r;
                    } else if (!r.is_none()) {
                        if (first_big) {
                            bits = r.bits;
                            first_big = false;
                        } else if (r.bits < bits) {
                            bits = r.bits;
                        }
                    }
                }

                return Precision(bits);
            });
        }

        case MachineComplexType:
        case BigComplexType:
            // TODO

        default:
            return Precision::none;
    }
}

void Expression::symbolic_initialize(
	const std::function<SymEngineRef()> &f,
	const Evaluation &evaluation) const {

	m_symbolic_form.ensure([&f, &evaluation] () {
		try {
			SymEngineRef ref = f();
			if (ref.get()) {
				return SymbolicForm::construct(ref);
			} else {
				return evaluation.no_symbolic_form;
			}
		} catch (const SymEngine::SymEngineException &e) {
			evaluation.sym_engine_exception(e);
			return evaluation.no_symbolic_form;
		}
	});
}

BaseExpressionRef Expression::symbolic_evaluate_unary(
	const SymEngineUnaryFunction &f,
	const Evaluation &evaluation) const {

	const SymbolicFormRef form = m_symbolic_form.ensure([this, &f, &evaluation] () {
		if (size() == 1) {
			try {
				const SymbolicFormRef symbolic_a = unsafe_symbolic_form(
					n_leaves<1>()[0], evaluation);
				if (!symbolic_a->is_none()) {
					return SymbolicForm::construct(f(symbolic_a->get()));
				} else {
					return evaluation.no_symbolic_form;
				}
			} catch (const SymEngine::SymEngineException &e) {
				evaluation.sym_engine_exception(e);
				return evaluation.no_symbolic_form;
			}
		} else {
			return evaluation.no_symbolic_form;
		}
	});


	if (form->is_none()) {
		return BaseExpressionRef();
	} else {
		BaseExpressionRef expr = from_symbolic_form(
			form->get(), evaluation);
#if DEBUG_SYMBOLIC
		std::cout << "sym form " << debugform() << " -> " << expr->debugform() << std::endl;
#endif
		if (!expr->same(this)) {
			return expr;
		} else {
			set_symbolic_form(form->get());
			return BaseExpressionRef();
		}
	}
}

BaseExpressionRef Expression::symbolic_evaluate_binary(
	const SymEngineBinaryFunction &f,
	const Evaluation &evaluation) const {

	const SymbolicFormRef form = m_symbolic_form.ensure([this, &f, &evaluation] () {
		if (size() == 2) {
			try {
				const BaseExpressionRef *const leaves = n_leaves<2>();
				const BaseExpressionRef &a = leaves[0];
				const BaseExpressionRef &b = leaves[1];

				const SymbolicFormRef symbolic_a = unsafe_symbolic_form(a, evaluation);
				if (!symbolic_a->is_none()) {
					const SymbolicFormRef symbolic_b = unsafe_symbolic_form(b, evaluation);
					if (!symbolic_b->is_none()) {
						return SymbolicForm::construct(f(symbolic_a->get(), symbolic_b->get()));
					}
				}

				return evaluation.no_symbolic_form;
			} catch (const SymEngine::SymEngineException &e) {
				evaluation.sym_engine_exception(e);
				return evaluation.no_symbolic_form;
			}
		} else {
			return evaluation.no_symbolic_form;
		}
	});

	if (form->is_none()) {
		return BaseExpressionRef();
	} else {
		BaseExpressionRef expr = from_symbolic_form(
			form->get(), evaluation);
		if (!expr->same(this)) {
			return expr;
		} else {
			set_symbolic_form(form->get());
			return BaseExpressionRef();
		}
	}
}

BaseExpressionRef BaseExpression::negate(const Evaluation &evaluation) const {
    return expression(evaluation.Times, evaluation.minus_one, BaseExpressionRef(this));
}

std::string BaseExpression::debug(const Evaluation &evaluation) const {
    return evaluation.format_output(this);
}
