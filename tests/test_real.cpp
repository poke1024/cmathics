#include <stdlib.h>
#include <gtest/gtest.h>
#include <mpfr.h>

#include "core/types.h"
#include "core/real.h"
#include "core/integer.h"


TEST(MachineReal, MachineReal_init) {
    MachineReal p(0.);
    EXPECT_EQ(p.type(), MachineRealType);
    EXPECT_EQ(p.value, 0.0);
}


TEST(MachineReal, MachineReal_set) {
    MachineReal p(1.5);
    EXPECT_EQ(p.value, 1.5);
}


/*TEST(MachineReal, MachineReal_from_d) {
    MachineReal* p = MachineReal_from_d(1.5);
    EXPECT_EQ(p->base.type, MachineRealType);
    EXPECT_EQ(p->base.ref, 0);
    EXPECT_EQ(p->value, 1.5);
}*/


TEST(BigReal, BigReal_init) {
    BigReal p(123.5, 0.0);
    EXPECT_EQ(p.type(), BigRealType);
    ASSERT_TRUE(p.value != NULL);
    EXPECT_EQ(mpfr_cmp_d(p.value, 0.0), 0);
    EXPECT_EQ(p.prec, 123.5);
    EXPECT_EQ(mpfr_get_prec(p.value), 411);
    // EXPECT_TRUE(mpfr_nan_p(p.value) != 0);
}


TEST(BigReal, BigReal_set) {
    BigReal p(30, 1.5);
    EXPECT_EQ(mpfr_get_d(p.value, MPFR_RNDN), 1.5);
}


TEST(PrecisionOf, MachineReal) {
    MachineReal p(1.5);
    auto ret = precision_of((BaseExpressionPtr)&p);
    EXPECT_EQ(ret.first, 1);
    EXPECT_EQ(ret.second, 0.0);
}


TEST(PrecisionOf, BigReal) {
    BigReal p(30.0, 1.5);
    auto ret = precision_of((BaseExpressionPtr)&p);
    EXPECT_EQ(ret.first, 2);
    EXPECT_EQ(ret.second, 30.0);
}


TEST(PrecisionOf, MachineInteger) {
    MachineInteger p(5);
    auto ret = precision_of((BaseExpressionPtr)&p);
    EXPECT_EQ(ret.first, 0);
}


TEST(PrecisionOf, BigInteger) {
    double prec;
    int32_t ret_code;
    mpz_t value;

    mpz_init_set_si(value, 5);

    BigInteger p(value);

    auto ret = precision_of((BaseExpressionPtr)&p);
    EXPECT_EQ(ret.first, 0);

    mpz_clear(value);
}


// TODO
TEST(PrecisionOf, Rational) {
}


// TODO
TEST(PrecisionOf, Complex) {
}


// TODO
TEST(PrecisionOf, Expression) {
}


// TODO
TEST(PrecisionOf, Symbol) {
}


// TODO
TEST(PrecisionOf, String) {
}


// TODO
TEST(PrecisionOf, RawExpression) {
}
