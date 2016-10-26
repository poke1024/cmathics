#include <stdlib.h>
#include <gmpxx.h>
#include <gtest/gtest.h>


#include "core/types.h"
#include "core/integer.h"


TEST(MachineInteger, MachineInteger_init) {
    MachineInteger p(0);
    EXPECT_EQ(p.type(), MachineIntegerType);
    EXPECT_EQ(p.value, 0);
}


TEST(MachineInteger, MachineInteger_set) {
    MachineInteger p(2);
    EXPECT_EQ(p.value, 2);
}


TEST(BigInteger, BigInteger_new) {
    mpz_t value;
    mpz_init_set_si(value, 0);

    BigInteger p(value);
    EXPECT_EQ(p.type(), BigIntegerType);
    EXPECT_EQ(mpz_cmp_si(p.value, 0), 0);
}


TEST(BigInteger, BigInteger_set__small) {
    mpz_t value;
    mpz_init_set_si(value, 5);

    BigInteger p(value);
    EXPECT_EQ(mpz_cmp_si(p.value, 5), 0);

    // ensure independent of original mpz_t
    mpz_set_si(value, 6);
    EXPECT_EQ(mpz_cmp_si(p.value, 5), 0);
    mpz_clear(value);
    EXPECT_EQ(mpz_cmp_si(p.value, 5), 0);
}

TEST(BigInteger, BigInteger_set__big) {
    mpz_t value;
    const char* hex_value = "f752d912b1bd0ed02b0632469e0bf641ca52f36d0b4cbda9c1051ff2975b515fce7b0c9";

    mpz_init_set_si(value, 41);
    mpz_pow_ui(value, value, 53);  // overflows 64bit int for sure
    ASSERT_STREQ(mpz_get_str(NULL, 16, value), hex_value);

    BigInteger p(value);
    EXPECT_EQ(mpz_cmp(p.value, value), 0);

    mpz_set_si(value, 0);
    EXPECT_STREQ(mpz_get_str(NULL, 16, p.value), hex_value);

    mpz_clear(value);
    EXPECT_STREQ(mpz_get_str(NULL, 16, p.value), hex_value);
}


TEST(Integer_from_mpz, MachineInteger) {
    mpz_t value;
    mpz_init_set_si(value, 5);
    auto result = Integer_from_mpz(value);
    ASSERT_EQ(result->type(), MachineIntegerType);
    EXPECT_EQ(std::static_pointer_cast<const MachineInteger>(result)->value, 5);
}


TEST(Integer_from_mpz, BigInteger) {
    mpz_t value;
    const char* hex_value = "f752d912b1bd0ed02b0632469e0bf641ca52f36d0b4cbda9c1051ff2975b515fce7b0c9";
    mpz_init_set_si(value, 41);
    mpz_pow_ui(value, value, 53);  // overflows 64bit int for sure

    auto result = Integer_from_mpz(value);
    ASSERT_EQ(result->type(), BigIntegerType);
    EXPECT_STREQ(mpz_get_str(NULL, 16, std::static_pointer_cast<const BigInteger>(result)->value), hex_value);
}
