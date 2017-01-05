#ifndef REAL_H
#define REAL_H

#include <stdint.h>
#include <sstream>
#include <iomanip>

#include <arb.h>
#include <symengine/eval_arb.h>
#include <symengine/real_double.h>
#include <symengine/real_mpfr.h>

#include "types.h"
#include "hash.h"

inline bool is_almost_equal(arb_t x, int px, arb_t y, int py) {
    // considers numbers equal that differ in their last 7 binary digits
    arb_t tx, ty;
    arb_init(tx);
    arb_init(ty);
    arb_set_round(tx, x, px - 7);
    arb_set_round(tx, y, py - 7);
    bool eq = arb_overlaps(tx, ty);
    arb_clear(tx);
    arb_clear(ty);
    return eq;
}

class MachineReal : public BaseExpression {
private:
    static const std::hash<machine_real_t> hash_function;

public:
	static constexpr Type Type = MachineRealType;

    const machine_real_t value;

    explicit MachineReal(machine_real_t new_value) :
	    BaseExpression(MachineRealExtendedType), value(new_value) {
    }

    virtual BaseExpressionPtr head(const Evaluation &evaluation) const final;

    virtual inline bool same(const BaseExpression &expr) const final {
        if (expr.type() == MachineRealType) {
            return value == static_cast<const MachineReal*>(&expr)->value;
        } else {
            return false;
        }
    }

    /*virtual bool equals(const BaseExpression &expr) const final {
        switch (expr.type()) {
            case MachineRealType: {
                arb_init(x);
                arb_set_d(x, value);
                std::numerical_limits<machine_real_t>::digits
                return false;
            }
            default:
                return false;
        }
    }*/

    virtual hash_t hash() const {
        // TODO better hash see CPython _Py_HashDouble
        return hash_pair(machine_real_hash, hash_function(value));
    }

    virtual std::string fullform() const {
        std::ostringstream s;
        s << std::showpoint << std::setprecision(6) << value;
        return s.str();
    }

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }

    virtual double round_to_float() const {
        return value;
    }

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
        return Pool::SymbolicForm(SymEngine::real_double(value));
    }
};

class Precision {
private:
   static inline mp_prec_t to_bits_prec(double prec) {
        constexpr double LOG_2_10 = 3.321928094887362; // log2(10.0);
        return static_cast<mp_prec_t>(ceil(LOG_2_10 * prec));
    }

    static inline mp_prec_t from_bits_prec(mp_prec_t bits_prec) {
        constexpr double LOG_2_10 = 3.321928094887362; // log2(10.0);
        return bits_prec / LOG_2_10;
    }

public:
    inline explicit Precision(double decimals_) : decimals(decimals_), bits(to_bits_prec(decimals_)) {
    }

    inline explicit Precision(long bits_) : bits(bits_), decimals(from_bits_prec(bits_)) {
    }

    inline Precision(const Precision &p) : decimals(p.decimals), bits(p.bits) {
    }

    const double decimals;
    const long bits;
};

class BigReal : public BaseExpression {
public:
	static constexpr Type Type = BigRealType;

    arb_t value;
    const Precision prec;

    explicit inline BigReal(arb_t value_, const Precision &prec_) :
        BaseExpression(BigRealExtendedType), prec(prec_) {

        arb_swap(value, value_);
    }

	explicit inline BigReal(double value_, const Precision &prec_) :
		BaseExpression(BigRealExtendedType), prec(prec_) {

        arb_init(value);
        arb_set_d(value, value_);
    }

    explicit inline BigReal(const SymbolicFormRef &form, const Precision &prec_) :
        BaseExpression(BigRealExtendedType), prec(prec_) {

        arb_init(value);
        SymEngine::eval_arb(value, *form->get().get(), prec.bits);
    }

    virtual ~BigReal() {
        arb_clear(value);
    }

    virtual BaseExpressionPtr head(const Evaluation &evaluation) const final;

	virtual inline bool same(const BaseExpression &expr) const final {
        if (expr.type() == BigRealType) {
            return arb_equal(value, static_cast<const BigReal*>(&expr)->value); // FIXME
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        // TODO hash
        return 0;
    }

    virtual std::string fullform() const {
	    std::ostringstream s;
        s << arb_get_str(value, long(floor(prec.decimals)), ARB_STR_NO_RADIUS);
        return s.str();
    }

    inline double as_double() const {
        return arf_get_d(arb_midref(value), ARF_RND_DOWN); // FIXME
    }

    virtual double round_to_float() const {
        return as_double();
    }

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
        // FIXME; this is inexact, as we only use the midpoint

        SymEngine::mpfr_class x;
        arf_get_mpfr(x.get_mpfr_t(), arb_midref(value), MPFR_RNDN);
        return Pool::SymbolicForm(SymEngine::real_mpfr(x));
    }
};

std::pair<int32_t,double> precision_of(const BaseExpressionRef&);

#endif
