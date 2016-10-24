#include <stdlib.h>
#include <gtest/gtest.h>


#include "core/types.h"
#include "core/string.h"


/*TEST(String, String_new) {
    String* p;
    p = String_new(2);
    EXPECT_TRUE(p != NULL);
    String_free(p);
}*/


TEST(String, String_init) {
    String p("");
    EXPECT_EQ(p.type(), StringType);
}


TEST(String, String_set) {
    String p("abcde");
    EXPECT_STREQ(p.value.c_str(), "abcde");
}
