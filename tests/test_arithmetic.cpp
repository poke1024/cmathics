#include <stdlib.h>
#include <gmpxx.h>
#include <gtest/gtest.h>


#include "core/types.h"
#include "core/integer.h"
#include "core/arithmetic.h"
#include "core/definitions.h"


TEST(Arithmetic, Plus) {
    // initialize arguments
    MachineInteger *a = new MachineInteger(1);
    MachineInteger *b = new MachineInteger(2);
    MachineInteger* c;

    // get plus head
    Definitions* definitions = new Definitions(); // System Definitions
    BaseExpression* plus_head = (BaseExpression*) definitions->lookup("Plus");

    // construct expression
    Expression* plus_expr = new Expression(plus_head, {a, b});

    BaseExpressionPtr result_expr = _Plus(plus_expr);
    ASSERT_EQ(result_expr->type(), MachineIntegerType);
    c = (MachineInteger*) result_expr;
    EXPECT_EQ(c->value, 3);
}


