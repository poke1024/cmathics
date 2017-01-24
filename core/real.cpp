#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "types.h"
#include "real.h"
#include "expression.h"

const std::hash<machine_real_t> MachineReal::hash_function = std::hash<machine_real_t>();

inline bool is_almost_equal(machine_real_t s, machine_real_t t, machine_real_t rel_eps) {
	// adapted from mpmath.almosteq()

	const machine_real_t diff = std::abs(s - t);
	const machine_real_t abss = std::abs(s);
	const machine_real_t abst = std::abs(t);
	machine_real_t err;
	if (abss < abst) {
		err = diff / abst;
	} else {
		err = diff / abss;
	}
	return err <= rel_eps;
}

inline bool is_almost_equal(const arf_t &s, const arf_t &t, const arf_t &rel_eps, int prec) {
	// adapted from mpmath.almosteq()

	arf_t diff;
	arf_init(diff);
	arf_sub(diff, s, t, prec, ARF_RND_NEAR);
	arf_abs(diff, diff);

	arf_t abss;
	arf_init(abss);
	arf_abs(abss, s);

	arf_t abst;
	arf_init(abst);
	arf_abs(abst, t);

	arf_t err;
	arf_init(err);

	if (arf_cmp(abss, abst) < 0) {
		arf_div(err, diff, abst, prec, ARF_RND_NEAR);
	} else {
		arf_div(err, diff, abss, prec, ARF_RND_NEAR);
	}

	arf_clear(diff);
	arf_clear(abss);
	arf_clear(abst);

	const bool eq = arf_cmp(err, rel_eps) <= 0;

	arf_clear(err);

	return eq;
}

inline bool is_almost_equal(machine_real_t s, const BigReal &t) {
	constexpr int s_prec = std::numeric_limits<machine_real_t>::digits;
	const int t_prec = t.prec.bits;
	const int prec = std::min(s_prec, t_prec);

	arf_t rel_eps;
	arf_init(rel_eps);
	arf_set_si_2exp_si(rel_eps, 1, -(prec - 7));

	arf_t su;
	arf_init(su);
	arf_set_d(su, s);

	arf_t tu, tv;
	arf_init(tu);
	arf_init(tv);
	arb_get_interval_arf(tu, tv, t.value, prec);

	const bool eq = is_almost_equal(su, tu, rel_eps, prec) &&
		is_almost_equal(su, tv, rel_eps, prec);

	arf_clear(tu);
	arf_clear(tv);
	arf_clear(su);

	arf_clear(rel_eps);

	return eq;
}

BaseExpressionPtr MachineReal::head(const Evaluation &evaluation) const {
	return evaluation.Real;
}

std::string MachineReal::format(const SymbolRef &form, const Evaluation &evaluation) const {
	std::ostringstream s;
	s << std::showpoint << std::setprecision(6) << value;
	return s.str();
}

bool MachineReal::equals(const BaseExpression &expr) const {
	switch (expr.type()) {
		case MachineRealType: {
			const machine_real_t s = value;
			const machine_real_t t = static_cast<const MachineReal*>(&expr)->value;

			constexpr int prec = std::numeric_limits<machine_real_t>::digits;
			static const machine_real_t rel_eps = std::pow(0.5, prec - 7);

			return is_almost_equal(s, t, rel_eps);
		}

		case BigRealType: {
			const BigReal &t = *static_cast<const BigReal*>(&expr);
			return is_almost_equal(value, t);
		}

		default:
			return false;
	}
}

BaseExpressionPtr BigReal::head(const Evaluation &evaluation) const {
	return evaluation.Real;
}

std::string BigReal::format(const SymbolRef &form, const Evaluation &evaluation) const {
	std::ostringstream s;
	s << arb_get_str(value, long(floor(prec.decimals)), ARB_STR_NO_RADIUS);
	return s.str();
}

bool BigReal::equals(const BaseExpression &expr) const {
	const BigReal &s = *this;

	switch (expr.type()) {
		case BigRealType: {
			const BigReal &t = *static_cast<const BigReal*>(&expr);

			const int s_prec = s.prec.bits;
			const int t_prec = t.prec.bits;
			const int prec = std::min(s_prec, t_prec);

			arf_t rel_eps;
			arf_init(rel_eps);
			arf_set_si_2exp_si(rel_eps, 1, -(prec - 7));

			arf_t su, sv;
			arf_init(su);
			arf_init(sv);
			arb_get_interval_arf(su, sv, s.value, prec);

			arf_t tu, tv;
			arf_init(tu);
			arf_init(tv);
			arb_get_interval_arf(tu, tv, t.value, prec);

			const bool eq = is_almost_equal(su, tu, rel_eps, prec) &&
				is_almost_equal(sv, tv, rel_eps, prec);

			arf_clear(su);
			arf_clear(sv);
			arf_clear(tu);
			arf_clear(tv);

			arf_clear(rel_eps);

			return eq;
		}

		case MachineRealType: {
			const MachineReal &t = *static_cast<const MachineReal*>(&expr);
			return is_almost_equal(t.value, s);
		}

		default:
			return false;
	}
}

// determine precision of an expression
// returns 0 if the precision is infinite
// returns 1 if the precision is machine-valued
// returns 2 if the precision is big in which case result is also set

std::pair<int32_t,double> precision_of(const BaseExpressionRef &expr) {
	return std::pair<int32_t,double>(0,  0.0); // FIXME

	/*switch (expr->type()) {
		case MachineRealType:
			return std::pair<int32_t,double>(1, 0.0);
		case BigRealType: {
			auto big_expr = (const BigReal *) expr.get();
			return std::pair<int32_t, double>(2, big_expr->_prec);
		}
		case ExpressionType: {
			bool first_big = false;
			const Expression *expr_expr = static_cast<const Expression*>(expr.get());
			double result = 0.0;
			for (auto leaf : *expr_expr) {
				auto r = precision_of(leaf);
				auto ret_code = r.first;
				auto leaf_result = r.second;

				if (ret_code == 1) {
					return std::pair<int32_t, double>(1, result);
				} else if (ret_code == 2) {
					if (first_big) {
						result = leaf_result;
						first_big = false;
					} else {
						if (leaf_result < result) {
							result = leaf_result;
						}
					}
				} else {
					assert(ret_code == 0);
				}
			}
			if (first_big) {
				return std::pair<int32_t, double>(0, result);
			} else {
				return std::pair<int32_t, double>(2, result);
			}
		}
		case ComplexType:
			// TODO
		default:
			return std::pair<int32_t,double>(0,  0.0);
	}*/
}
