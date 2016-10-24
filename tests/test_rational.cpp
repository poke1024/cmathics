#include <stdlib.h>
#include <gtest/gtest.h>
#include <gmpxx.h>

#include "core/types.h"
#include "core/integer.h"
#include "core/rational.h"


TEST(Rational, Rational_init) {
    mpq_t value;
    mpq_init(value);
    mpq_set_str(value, "1/1", 10);

    Rational q = Rational(value);
    EXPECT_EQ(q.type(), RationalType);
}


TEST(Rational, Rational_set) {
    mpq_t value;
    mpq_init(value);
    mpq_set_str(value, "5/7", 10);

    Rational q(value);
    EXPECT_EQ(mpq_cmp(q.value, value), 0);
}

TEST(Rational, Rational_numer) {
    mpq_t value;
    mpq_init(value);
    mpq_set_si(value, 5, 7);
    Rational q(value);

    Integer *integer = q.numer();
    EXPECT_EQ(integer->type(), MachineIntegerType);
    EXPECT_EQ(((const MachineInteger*)integer)->value, 5);
}
