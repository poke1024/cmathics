#ifndef REAL_H
#define REAL_H

#include <stdint.h>
#include <sstream>
#include <iomanip>

#include <arb.h>
#include <symengine/eval_arb.h>

#include "types.h"
#include "hash.h"

class MachineReal : public BaseExpression {
public:
	static constexpr Type Type = MachineRealType;

    const machine_real_t value;

    explicit MachineReal(machine_real_t new_value) :
	    BaseExpression(MachineRealExtendedType), value(new_value) {
    }

    virtual bool same(const BaseExpression &expr) const {
        if (expr.type() == MachineRealType) {
            return value == static_cast<const MachineReal*>(&expr)->value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        // TODO better hash see CPython _Py_HashDouble
        return hash_pair(machine_real_hash, (uint64_t)value);
    }

    virtual std::string fullform() const {
        std::ostringstream s;
        s << std::showpoint << std::setprecision(
            std::numeric_limits<machine_real_t>::digits10) << value;
        return s.str();
    }

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
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

    explicit inline BigReal(const SymbolicForm &form, const Precision &prec_) :
        BaseExpression(BigRealExtendedType), prec(prec_) {

        arb_init(value);
        SymEngine::eval_arb(value, *form.get(), prec.bits);
    }

    virtual ~BigReal() {
        arb_clear(value);
    }

	virtual bool same(const BaseExpression &expr) const {
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
};

inline BaseExpressionRef from_primitive(machine_real_t value) {
	return Heap::MachineReal(value);
}

std::pair<int32_t,double> precision_of(const BaseExpressionRef&);

#endif
