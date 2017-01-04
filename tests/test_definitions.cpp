/*#include <stdlib.h>
#include <gtest/gtest.h>

#include "core/definitions.h"
#include "core/expression.h"
#include "core/types.h"
*/

/*TEST(Definitions, new32) {
    Definitions* d = new Definitions();
    ASSERT_TRUE(d != NULL);
    EXPECT_EQ(d->size, 32);
    EXPECT_EQ(d->count, 0);
    Definitions_free(d);
}


TEST(Definitions, new0) {
    Definitions* d = new Definitions();
    EXPECT_TRUE(d == NULL);
}*/

/*
TEST(Definitions, init) {
    Definitions* d = new Definitions();
    SymbolRef l = d->lookup("List");
    ASSERT_TRUE(l != NULL);
    EXPECT_STREQ(l->name().c_str(), "List");
}


TEST(Definitions, lookup) {
    Definitions* d = new Definitions();
    SymbolRef s = d->lookup("abc");
    ASSERT_TRUE(s != NULL);
    EXPECT_STREQ(s->name().c_str(), "abc");
}


TEST(Definitions, lookup_twice) {
    Definitions* d = new Definitions();
    SymbolRef s1 = d->lookup("abc");
    SymbolRef s2 = d->lookup("abc");
    ASSERT_TRUE(s1 != NULL);
    ASSERT_TRUE(s2 != NULL);
    EXPECT_EQ(s1, s2);
}*/
