#ifndef REAL_H
#define REAL_H

#include <stdint.h>
#include <sstream>
#include <iomanip>

#include <arb.h>
#include <symengine/eval.h>
#include <symengine/eval_double.h>
#include <symengine/real_double.h>
#include <symengine/real_mpfr.h>

#include "core/types.h"
#include "core/hash.h"

inline machine_real_t chop(machine_real_t x) {
    // chop off the last 8 bits of the mantissa to make this safe for hashing
    // and using equals() later on.

    constexpr int mantissa_size = std::numeric_limits<machine_real_t>::digits;

    int exp;
    machine_real_t mantissa_flt = std::frexp(x, &exp);

    constexpr double ignore = 1L << 8;

    mantissa_flt = std::scalbn(mantissa_flt, mantissa_size);
    mantissa_flt = std::floor(mantissa_flt / ignore) * ignore;
    mantissa_flt = std::scalbn(mantissa_flt, -mantissa_size);

    return std::ldexp(mantissa_flt, exp);
}

inline machine_real_t eval_to_machine_real(const SymbolicFormRef &form) {
    if (sizeof(machine_real_t) == sizeof(double)) {
        return SymEngine::eval_double(*form->get().get());
    } else {
	    assert(false);
        // SymEngine::evalf(*form->get().get(), std::numeric_limits<machine_real_t>::digits, true);
    }
}

class MachineReal : public BaseExpression {
private:
    static const std::hash<machine_real_t> hash_function;

public:
	static constexpr Type Type = MachineRealType;

    const machine_real_t value;

    explicit inline MachineReal(machine_real_t new_value) :
	    BaseExpression(MachineRealExtendedType), value(new_value) {
    }

    explicit inline MachineReal(const SymbolicFormRef &form) :
        BaseExpression(MachineRealExtendedType), value(eval_to_machine_real(form)) {
    }

    virtual std::string debugform() const;

    virtual BaseExpressionRef make_boxes(
        BaseExpressionPtr form,
        const Evaluation &evaluation) const;

	std::string boxes_to_text(const StyleBoxOptions &options, const Evaluation &evaluation) const;

    virtual BaseExpressionPtr head(const Symbols &symbols) const final;

    virtual inline bool same(const BaseExpression &expr) const final {
        if (expr.is_machine_real()) {
            return value == static_cast<const MachineReal*>(&expr)->value;
        } else {
            return false;
        }
    }

    virtual tribool equals(const BaseExpression &expr) const final;

    virtual hash_t hash() const {
        return hash_pair(machine_real_hash, hash_function(chop(value)));
    }

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }

    virtual double round_to_float() const {
        return value;
    }

    virtual bool is_numeric() const {
        return true;
    }

    virtual bool is_negative() const {
        return value < 0.;
    }

    virtual bool is_inexact() const final {
        return true;
    }

    virtual BaseExpressionRef negate(const Evaluation &evaluation) const final;

    virtual optional<SExp> to_s_exp(optional<machine_integer_t> &n) const final;

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
        return Pool::SymbolicForm(SymEngine::real_double(value));
    }
};

class BigReal : public BaseExpression {
public:
	static constexpr Type Type = BigRealType;

    mpfr_t value;
    const Precision prec;

    explicit inline BigReal(mpfr_t value_, const Precision &prec_) :
        BaseExpression(BigRealExtendedType), prec(prec_) {

	    mpfr_swap(value, value_);
    }

	explicit inline BigReal(double value_, const Precision &prec_) :
		BaseExpression(BigRealExtendedType), prec(prec_) {

		mpfr_init2(value, prec.bits);
        mpfr_set_d(value, value_, MPFR_RNDN);
    }

    virtual ~BigReal() {
        mpfr_clear(value);
    }

    virtual std::string debugform() const;

    virtual BaseExpressionRef make_boxes(
        BaseExpressionPtr form,
        const Evaluation &evaluation) const;

    std::string boxes_to_text(const StyleBoxOptions &options, const Evaluation &evaluation) const;

    virtual BaseExpressionPtr head(const Symbols &symbols) const final;

	virtual inline bool same(const BaseExpression &expr) const final {
        if (expr.is_big_real()) {
            return mpfr_cmp(value, static_cast<const BigReal*>(&expr)->value) == 0;
        } else {
            return false;
        }
    }

    virtual tribool equals(const BaseExpression &expr) const final;

    virtual hash_t hash() const {
        return 0; // FIXME
    }

    inline double as_double() const {
        return mpfr_get_d(value, MPFR_RNDN);
    }

    virtual double round_to_float() const {
        return as_double();
    }

    virtual bool is_numeric() const {
        return true;
    }

    virtual bool is_negative() const {
        return mpfr_cmp_si(value, 0) < 0;
    }

    virtual bool is_inexact() const final {
        return true;
    }

    virtual BaseExpressionRef negate(const Evaluation &evaluation) const;

    virtual optional<SExp> to_s_exp(optional<machine_integer_t> &n) const final;

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
        SymEngine::mpfr_class x(value);
        return Pool::SymbolicForm(SymEngine::real_mpfr(x));
    }
};

inline BaseExpressionRef eval(
	const SymbolicFormRef &form,
	const Precision &prec,
	const Evaluation &evaluation) {

	return from_symbolic_form(SymEngine::evalf(*form->get().get(), prec.bits + 2, true), evaluation);
}

#endif
