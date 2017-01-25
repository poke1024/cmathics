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

    virtual bool equals(const BaseExpression &expr) const final;

    virtual hash_t hash() const {
        // TODO better hash see CPython _Py_HashDouble
        return hash_pair(machine_real_hash, hash_function(value));
    }

    virtual std::string format(const SymbolRef &form, const Evaluation &evaluation) const final;

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }

    virtual double round_to_float() const {
        return value;
    }

    virtual bool is_inexact() const final {
        return true;
    }

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
        return Pool::SymbolicForm(SymEngine::real_double(value), true);
    }
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

    virtual bool equals(const BaseExpression &expr) const final;

    virtual hash_t hash() const {
        // TODO hash
        return 0;
    }

    virtual std::string format(const SymbolRef &form, const Evaluation &evaluation) const final;

    inline double as_double() const {
        return arf_get_d(arb_midref(value), ARF_RND_DOWN); // FIXME
    }

    virtual double round_to_float() const {
        return as_double();
    }

    virtual bool is_inexact() const final {
        return true;
    }

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
        // FIXME; this is inexact, as we only use the midpoint

        SymEngine::mpfr_class x;
        arf_get_mpfr(x.get_mpfr_t(), arb_midref(value), MPFR_RNDN);
        return Pool::SymbolicForm(SymEngine::real_mpfr(x), true);
    }
};

#endif
