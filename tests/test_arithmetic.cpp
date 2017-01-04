// @formatter:off

#include "../core/types.h"
#include "../core/runtime.h"
#include "../tests/doctest.h"

TEST_CASE("Plus") {
    auto &definitions = Runtime::get()->definitions();

    // initialize arguments
    BaseExpressionRef a = Pool::MachineInteger(1);
    BaseExpressionRef b = Pool::MachineInteger(2);

    // get plus head
    auto plus_head = definitions.lookup("System`Plus");

    // construct expression
    ExpressionRef plus_expr = expression(plus_head, {a, b});

    Evaluation evaluation(definitions, false);
    BaseExpressionRef result_expr = plus_expr->evaluate_or_copy(evaluation);

    CHECK(result_expr->type() == MachineIntegerType);
    CHECK(static_pointer_cast<const MachineInteger>(result_expr)->value == 3);
}
