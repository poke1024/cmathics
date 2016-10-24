#include <stdlib.h>
#include <gmpxx.h>
#include <gtest/gtest.h>

#include "core/types.h"
#include "core/integer.h"


/*TEST(List, List_new) {
    List l;
    List_new(&l, 5);
    EXPECT_EQ(l.size, 5);
    EXPECT_EQ(l.count, 0);
    List_free(&l);
}


TEST(List, List_append) {
    List l;
    MachineInteger item;
    BaseExpression* p;

    MachineInteger_init(&item);
    p = (BaseExpression*) &item;

    List_new(&l, 5);
    List_append(&l, p);
    EXPECT_EQ(l.size, 5);
    EXPECT_EQ(l.count, 1);
    List_free(&l);
}


TEST(List, List_pop) {
    List l;
    MachineInteger item;
    BaseExpression* q;
    BaseExpression* p;

    MachineInteger_init(&item);
    p = (BaseExpression*) &item;

    List_new(&l, 3);
    List_append(&l, p);

    q = List_pop(&l);

    EXPECT_EQ(l.size, 3);
    EXPECT_EQ(l.count, 0);
    EXPECT_EQ(q, p);
    EXPECT_EQ(q->type, MachineIntegerType);
    List_free(&l);
}


TEST(List, List_pop_empty) {
    List l;
    List_new(&l, 5);
    EXPECT_TRUE(List_pop(&l) == NULL);
    List_free(&l);
}


TEST(List, List_grow) {
    List l;
    const int n = 5;

    List_new(&l, n);
    for (int i = 0; i < n; i ++) {
        List_append(&l, NULL);
    }

    EXPECT_EQ(l.size, 5);
    EXPECT_EQ(l.count, 5);

    List_append(&l, NULL);
    EXPECT_EQ(l.size, 10);
    EXPECT_EQ(l.count, 6);
}


TEST(List, List_shrink) {
    List l;

    List_new(&l, 20);
    for (int i = 0; i < 6; i ++) {
        List_append(&l, NULL);
    }

    EXPECT_EQ(l.size, 20);
    EXPECT_EQ(l.count, 6);

    List_pop(&l);

    EXPECT_EQ(l.size, 20);
    EXPECT_EQ(l.count, 5);

    List_pop(&l);

    EXPECT_EQ(l.size, 10);
    EXPECT_EQ(l.count, 4);

    List_pop(&l);

    EXPECT_EQ(l.size, 10);
    EXPECT_EQ(l.count, 3);

    List_pop(&l);

    EXPECT_EQ(l.size, 10);
    EXPECT_EQ(l.count, 2);

    List_pop(&l);

    EXPECT_EQ(l.size, 5);
    EXPECT_EQ(l.count, 1);

    List_pop(&l);
    EXPECT_EQ(l.size, 2);
    EXPECT_EQ(l.count, 0);
}*/
