// @formatter:off

#include "../core/types.h"
#include "../core/runtime.h"
#include "../tests/doctest.h"

TEST_CASE("Plus") {
    auto &definitions = Runtime::get()->definitions();

    // initialize arguments
    BaseExpressionRef a = MachineInteger::construct(1);
    BaseExpressionRef b = MachineInteger::construct(2);

    // get plus head
    auto plus_head = definitions.lookup("System`Plus");

    // construct expression
    ExpressionRef plus_expr = expression(plus_head, {a, b});

    const auto output = std::make_shared<TestOutput>();

    Evaluation evaluation(output, definitions, false);
    BaseExpressionRef result_expr = plus_expr->evaluate_or_copy(evaluation);

    CHECK(result_expr->is_machine_integer());
    CHECK(static_pointer_cast<const MachineInteger>(result_expr)->value == 3);
    CHECK(output->test_empty() == true);
}
