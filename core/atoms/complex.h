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

struct ComplexComponents {
	BaseExpressionRef real;
	BaseExpressionRef imag;
};

class MachineComplex : public BaseExpression, public PoolObject<MachineComplex> {
private:
    static const std::hash<machine_real_t> hash_function;

public:
    static constexpr Type Type = MachineComplexType;

    const std::complex<machine_real_t> m_value;
    mutable Spinlocked<optional<ComplexComponents>> m_components;

    explicit MachineComplex(machine_real_t real, machine_real_t imag) :
        BaseExpression(MachineComplexExtendedType), m_value(real, imag) {
    }

    virtual std::string debugform() const;

    virtual BaseExpressionRef custom_format(
        const BaseExpressionRef &form,
        const Evaluation &evaluation) const;

    virtual BaseExpressionPtr head(const Symbols &symbols) const final;

    virtual inline bool same_indeed(const BaseExpression &expr) const final {
        if (expr.is_machine_complex()) {
            return m_value == static_cast<const MachineComplex*>(&expr)->m_value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        const hash_t value_hash = hash_pair(
            hash_function(m_value.real()), hash_function(m_value.imag()));
        return hash_pair(machine_complex_hash, value_hash);
    }

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }

    virtual bool is_numeric() const {
        return true;
    }

    virtual bool is_inexact() const final {
        return true;
    }

    inline BaseExpressionRef conjugate() const {
        return MachineComplex::construct(m_value.real(), -m_value.imag());
    }

    virtual BaseExpressionRef negate(const Evaluation &evaluation) const final;

    virtual void sort_key(SortKey &key, const Evaluation &evaluation) const final;

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form(const Evaluation &evaluation) const final {
        return SymbolicForm::construct(SymEngine::complex_double(m_value));
    }
};

class BigComplex : public BaseExpression, public PoolObject<BigComplex> {
public:
    static constexpr Type Type = BigComplexType;

    const SymEngineComplexRef m_value;
	mutable Spinlocked<optional<ComplexComponents>> m_components;

    explicit inline BigComplex(const SymEngineComplexRef &value) :
        BaseExpression(BigComplexExtendedType), m_value(value) {
    }

	virtual std::string debugform() const;

    virtual BaseExpressionRef custom_format(
        const BaseExpressionRef &form,
        const Evaluation &evaluation) const;

    virtual bool same_indeed(const BaseExpression &expr) const {
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

    virtual bool is_inexact() const final {
        return false; // SymEngine's complex uses rationals
    }

    inline BaseExpressionRef conjugate() const {
        const SymEngine::Integer &minus_one = *SymEngine::minus_one;

        SymEngine::RCP<const SymEngine::Number> real = m_value->real_part();
        SymEngine::RCP<const SymEngine::Number> imag = m_value->imaginary_part()->mul(minus_one);

        return BigComplex::construct(SymEngine::rcp_static_cast<const SymEngine::Complex>(
            SymEngine::Complex::from_two_nums(*real, *imag)));
    }

    virtual BaseExpressionRef negate(const Evaluation &evaluation) const final;

	virtual void sort_key(SortKey &key, const Evaluation &evaluation) const final;

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form(const Evaluation &evaluation) const final {
        return SymbolicForm::construct(m_value);
    }
};

#endif //CMATHICS_COMPLEX_H
