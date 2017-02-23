#ifndef CMATHICS_COMPLEX_H
#define CMATHICS_COMPLEX_H

#include <stdint.h>
#include <sstream>
#include <iomanip>

#include <arb.h>
#include <symengine/eval_arb.h>
#include <symengine/real_double.h>
#include <symengine/real_mpfr.h>

#include "core/types.h"
#include "core/hash.h"

class MachineComplex : public BaseExpression {
private:
    static const std::hash<machine_real_t> hash_function;

public:
    static constexpr Type Type = MachineComplexType;

    const std::complex<machine_real_t> value;

    explicit MachineComplex(machine_real_t real, machine_real_t imag) :
        BaseExpression(MachineComplexExtendedType), value(real, imag) {
    }

    virtual std::string debugform() const;

    virtual BaseExpressionRef custom_format(
        const BaseExpressionRef &form,
        const Evaluation &evaluation) const;

    virtual BaseExpressionPtr head(const Symbols &symbols) const final;

    virtual inline bool same(const BaseExpression &expr) const final {
        if (expr.is_machine_complex()) {
            return value == static_cast<const MachineComplex*>(&expr)->value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        const hash_t value_hash = hash_pair(
            hash_function(value.real()), hash_function(value.imag()));
        return hash_pair(machine_complex_hash, value_hash);
    }

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }

    virtual bool is_numeric() const {
        return true;
    }

    virtual bool is_negative() const {
        return value.real() < 0. && value.imag() < 0.;
    }

    virtual bool is_inexact() const final {
        return true;
    }

    inline BaseExpressionRef conjugate() const {
        return Pool::MachineComplex(value.real(), -value.imag());
    }

    virtual BaseExpressionRef negate(const Evaluation &evaluation) const final;

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
        return Pool::SymbolicForm(SymEngine::complex_double(value));
    }
};

class BigComplex : public BaseExpression {
public:
    static constexpr Type Type = BigComplexType;

    //acb_t value;
    //const Precision prec;

    SymEngineComplexRef m_value;

    explicit inline BigComplex(const SymEngineComplexRef &value) :
        BaseExpression(BigComplexExtendedType), m_value(value) {
    }

	virtual std::string debugform() const;

    virtual BaseExpressionRef custom_format(
        const BaseExpressionRef &form,
        const Evaluation &evaluation) const;

    virtual bool same(const BaseExpression &expr) const {
        if (expr.is_big_complex()) {
            return m_value->__eq__(*static_cast<const BigComplex*>(&expr)->m_value.get());
        } else {
            return false;
        }
    }

    virtual tribool equals(const BaseExpression &expr) const {
        return same(expr);
    }

    virtual hash_t hash() const {
        return m_value->__hash__();
    }

    virtual BaseExpressionPtr head(const Symbols &symbols) const final;

    virtual bool is_numeric() const {
        return true;
    }

    virtual bool is_negative() const {
        return m_value->is_negative();
    }

    virtual bool is_inexact() const final {
        return false; // SymEngine's complex uses rationals
    }

    inline BaseExpressionRef conjugate() const {
        const SymEngine::Integer &minus_one = *SymEngine::minus_one;

        SymEngine::RCP<const SymEngine::Number> real = m_value->real_part();
        SymEngine::RCP<const SymEngine::Number> imag = m_value->imaginary_part()->mul(minus_one);

        return Pool::BigComplex(SymEngine::rcp_static_cast<const SymEngine::Complex>(
            SymEngine::Complex::from_two_nums(*real, *imag)));
    }

    virtual BaseExpressionRef negate(const Evaluation &evaluation) const final;

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
        return Pool::SymbolicForm(m_value);
    }
};

#endif //CMATHICS_COMPLEX_H
