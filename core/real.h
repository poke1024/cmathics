#ifndef REAL_H
#define REAL_H
#include <mpfr.h>
#include <stdint.h>

#include "types.h"
#include "hash.h"


class MachineReal : public BaseExpression {
public:
    const double value;

    MachineReal(const double new_value) : value(new_value) {
    }

    virtual Type type() const {
        return MachineRealType;
    }

    virtual bool same(const BaseExpression *expr) const {
        if (expr->type() == MachineRealType) {
            return value == static_cast<const MachineReal*>(expr)->value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        // TODO better hash see CPython _Py_HashDouble
        return hash_pair(machine_real_hash, (uint64_t)value);
    }

    virtual std::string fullform() const {
        return std::to_string(value); // FIXME
    }

    virtual Match match(const BaseExpression *expr) const {
        return Match(same(expr));
    }
};

class BigReal : public BaseExpression {
public:
    mpfr_t value;
    double prec;

    BigReal(const double new_prec, const double new_value);

    virtual ~BigReal() {
        mpfr_clear(value);
    }

    virtual Type type() const {
        return BigRealType;
    }

    virtual bool same(const BaseExpression *expr) const {
        if (expr->type() == BigRealType) {
            return mpfr_cmp(value, static_cast<const BigReal*>(expr)->value) == 0;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        // TODO hash
        return 0;
    }

    virtual std::string fullform() const {
        return std::string("bigreal"); // FIXME
    }
};

std::pair<int32_t,double> precision_of(const BaseExpressionRef&);

#endif
