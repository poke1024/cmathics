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

inline std::string rstrip(const std::string &s, char c) {
    size_t n = s.length();
    while (n > 0 && s[n - 1] == c) {
        n -= 1;
    }
    return s.substr(0, n);
}

inline SExp real_to_s_exp(
    std::string s,
    const optional<size_t> &n,
    bool is_machine_precision) {

    int non_negative;

    assert(s.length() > 0);
    if (s[0] == '-') {
        // assert(value < 0);
        non_negative = 0;
        s = s.substr(1);
    } else {
        // assert(value >= 0);
        non_negative = 1;
    }

    size_t exp;

    const auto e_pos = s.find('e');
    if (e_pos != std::string::npos) {
        exp = std::stoi(s.substr(e_pos + 1).c_str());
        s = s.substr(0, e_pos);
        if (s.length() > 1 && s[1] == '.') {
            s = s[0] + rstrip(s.substr(2), '0');
        }
    } else {
        size_t exp = index_t(s.find('.')) - 1;
        s = s.substr(0, exp + 1) + rstrip(s.substr(exp + 2), '0');

        // consume leading '0's.
        size_t i = 0;
        while (i < s.length() && s[i] == '0') {
            i += 1;
            exp -= 1;
        }
        s = s.substr(i);
    }

    if (n && !is_machine_precision && *n > s.length()) {
        s = s + std::string(*n - s.length(), '0');
    }

    return SExp(std::move(s), exp, non_negative);
}

SExp MachineReal::to_s_exp(const optional<size_t> &n) const {
    return real_to_s_exp(std::to_string(value), n, true);
}

std::string MachineReal::debugform() const {
	return std::to_string(value);
}

BaseExpressionRef MachineReal::make_boxes(
    BaseExpressionPtr form,
    const Evaluation &evaluation) const {

    std::ostringstream s;
    s << std::showpoint << std::setprecision(6) << value;
    return Pool::String(s.str());
}

std::string MachineReal::boxes_to_text(const Evaluation &evaluation) const {
	return make_boxes(evaluation.OutputForm, evaluation)->boxes_to_text(evaluation);
}

BaseExpressionPtr MachineReal::head(const Symbols &symbols) const {
	return symbols.Real;
}

tribool MachineReal::equals(const BaseExpression &expr) const {
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

BaseExpressionRef MachineReal::negate(const Evaluation &evaluation) const {
    return from_primitive(-value);
}

std::string BigReal::debugform() const {
	return std::string(arb_get_str(value, long(floor(prec.decimals)), ARB_STR_NO_RADIUS));
}

BaseExpressionRef BigReal::make_boxes(
    BaseExpressionPtr form,
    const Evaluation &evaluation) const {

    std::ostringstream s;
    s << arb_get_str(value, long(floor(prec.decimals)), ARB_STR_NO_RADIUS);
    return Pool::String(s.str());
}

std::string BigReal::boxes_to_text(const Evaluation &evaluation) const {
	return make_boxes(evaluation.OutputForm, evaluation)->boxes_to_text(evaluation);
}

BaseExpressionPtr BigReal::head(const Symbols &symbols) const {
	return symbols.Real;
}

tribool BigReal::equals(const BaseExpression &expr) const {
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

BaseExpressionRef BigReal::negate(const Evaluation &evaluation) const {
    arb_t negated;
    arb_init(negated);
    arb_neg(negated, value);
    return Pool::BigReal(negated, prec);
}

SExp BigReal::to_s_exp(const optional<size_t> &n) const {
    // FIXME
    assert(*n); // FIXME
    std::string s;
    mpfr_t x;
    arf_get_mpfr(x, arb_midref(value), MPFR_RNDN);
    mpfr_exp_t exp;
    char *t = mpfr_get_str(nullptr, &exp, 10, *n, x, MPFR_RNDN);
    try {
        s = t;
    } catch(...) {
        mpfr_free_str(t);
        throw;
    }
    mpfr_free_str(t);
    return real_to_s_exp(std::move(s), n, true);
}
