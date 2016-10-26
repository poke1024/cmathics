#include <stdlib.h>
#include <gmpxx.h>
#include <gtest/gtest.h>


#include "core/types.h"
#include "core/integer.h"
#include "core/arithmetic.h"
#include "core/definitions.h"


TEST(Arithmetic, Plus) {
    // initialize arguments
    BaseExpressionRef a = std::make_shared<MachineInteger>(1);
    BaseExpressionRef b = std::make_shared<MachineInteger>(2);

    // get plus head
    Definitions* definitions = new Definitions(); // System Definitions
    auto plus_head = definitions->lookup("Plus");

    // construct expression
    ExpressionRef plus_expr = ExpressionRef(new Expression(plus_head, {a, b}));

    BaseExpressionRef result_expr = _Plus(plus_expr);
    ASSERT_EQ(result_expr->type(), MachineIntegerType);
    EXPECT_EQ(std::static_pointer_cast<const MachineInteger>(result_expr)->value, 3);
}


