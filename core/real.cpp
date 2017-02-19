#include <stdlib.h>
#include <stdbool.h>
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

inline bool is_almost_equal(const mpfr_t &s, const mpfr_t &t, const mpfr_t &rel_eps, int prec) {
	// adapted from mpmath.almosteq()

	mpfr_t diff;
	mpfr_init2(diff, prec);
	mpfr_sub(diff, s, t, MPFR_RNDN);
	mpfr_abs(diff, diff, MPFR_RNDN);

	mpfr_t abss;
	mpfr_init2(abss, prec);
	mpfr_abs(abss, s, MPFR_RNDN);

	mpfr_t abst;
	mpfr_init2(abst, prec);
	mpfr_abs(abst, t, MPFR_RNDN);

	mpfr_t err;
	mpfr_init2(err, prec);

	if (mpfr_cmp(abss, abst) < 0) {
		mpfr_div(err, diff, abst, MPFR_RNDN);
	} else {
		mpfr_div(err, diff, abss, MPFR_RNDN);
	}

	mpfr_clear(diff);
	mpfr_clear(abss);
	mpfr_clear(abst);

	const bool eq = mpfr_cmp(err, rel_eps) <= 0;

	mpfr_clear(err);

	return eq;
}

inline bool is_almost_equal(machine_real_t s, const BigReal &t) {
	constexpr int s_prec = std::numeric_limits<machine_real_t>::digits;
	const int t_prec = t.prec.bits;
	const int prec = std::min(s_prec, t_prec);

	mpfr_t rel_eps;
	mpfr_init2(rel_eps, prec);
	mpfr_set_si_2exp(rel_eps, 1, -(prec - 7), MPFR_RNDN);

	mpfr_t su;
	mpfr_init2(su, prec);
	mpfr_set_d(su, s, MPFR_RNDN);

	const bool eq = is_almost_equal(su, t.value, rel_eps, prec);

	mpfr_clear(su);

	mpfr_clear(rel_eps);

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

    machine_integer_t exp;

    const auto e_pos = s.find('e');
    if (e_pos != std::string::npos) {
        exp = std::stoi(s.substr(e_pos + 1).c_str());
        s = s.substr(0, e_pos);
        if (s.length() > 1 && s[1] == '.') {
            s = s[0] + rstrip(s.substr(2), '0');
        }
    } else {
        exp = machine_integer_t(s.find('.')) - 1;
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

    return SExp(Pool::String(std::move(s)), exp, non_negative);
}

optional<SExp> MachineReal::to_s_exp(const optional<size_t> &n) const {
	std::string s;
	if (n) {
		assert(*n == 6);
		const char * const format = "%.5e";
		const size_t k = snprintf(nullptr, 0, format, value);
		std::unique_ptr<char[]> buffer(new char[k + 1]);
		snprintf(buffer.get(), k + 1, format, value);
		s = buffer.get();
	} else {
		s = std::to_string(value);
	}
    return real_to_s_exp(s, n, true);
}

std::string MachineReal::debugform() const {
	return std::to_string(value);
}

BaseExpressionRef MachineReal::make_boxes(
    BaseExpressionPtr form,
    const Evaluation &evaluation) const {

    optional<size_t> optional_n;

    switch (form->symbol()) {
	    case S::InputForm:
	    case S::FullForm:
            break;
        default:
            optional_n = 6;
            break;
    }

    const optional<SExp> s_exp = to_s_exp(optional_n);
	assert(s_exp);
    size_t n;

    if (optional_n) {
        n = *optional_n;
    } else {
        n = std::get<0>(*s_exp)->length();
    }

	const auto &number_form = evaluation.definitions.number_form;

	NumberFormOptions options;
	number_form.parse_options(evaluation.empty_list, options, evaluation);
    return evaluation.definitions.number_form(*s_exp, n, form, options, evaluation);
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
	const char * const format = "%RNg";

	const size_t k = mpfr_snprintf(nullptr, 0, format, value);
	std::unique_ptr<char[]> buffer(new char[k + 1]);
	mpfr_snprintf(buffer.get(), k + 1, format, value);

	return std::string(buffer.get());
}

BaseExpressionRef BigReal::make_boxes(
    BaseExpressionPtr form,
    const Evaluation &evaluation) const {

    const size_t n = std::floor(prec.decimals);
    const optional<SExp> s_exp = to_s_exp(n);
	assert(s_exp);

	const auto &number_form = evaluation.definitions.number_form;
	NumberFormOptions options;
	number_form.parse_options(evaluation.empty_list, options, evaluation);
	return evaluation.definitions.number_form(*s_exp, n, form, options, evaluation);
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

			mpfr_t rel_eps;
			mpfr_init2(rel_eps, prec);
			mpfr_set_si_2exp(rel_eps, 1, -(prec - 7), MPFR_RNDN);

			const bool eq = is_almost_equal(s.value, t.value, rel_eps, prec);

			mpfr_clear(rel_eps);

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
    mpfr_t negated;
	mpfr_init2(negated, prec.bits);
	mpfr_neg(negated, value, MPFR_RNDN);
    return Pool::BigReal(negated, prec);
}

optional<SExp> BigReal::to_s_exp(const optional<size_t> &n) const {
	const size_t t = n ? *n : prec.decimals;

    std::string s;
    const std::string format(
        std::string("%.") + std::to_string(t) + std::string("RNg"));

    const size_t k = mpfr_snprintf(nullptr, 0, format.c_str(), value);

    std::unique_ptr<char[]> buffer(new char[k + 1]);
    mpfr_snprintf(buffer.get(), k + 1, format.c_str(), value);
    s = buffer.get();

    return real_to_s_exp(std::move(s), n, true);
}
