#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <gmp.h>
#include <iostream>
#include <sstream>
#include <vector>

#include "core/misc.h"
#include "core/expression.h"
#include "core/definitions.h"
#include "core/formatter.h"
#include "core/evaluation.h"
#include "core/pattern.h"
#include "core/integer.h"
#include "core/real.h"
#include "core/rational.h"
#include "core/arithmetic.h"
#include "core/string.h"
#include "core/builtin.h"
#include "core/evaluate.h"
#include "core/parser.h"
#include "core/runtime.h"

#include "builtin/arithmetic.h"
#include "builtin/assignment.h"
#include "builtin/control.h"
#include "builtin/functional.h"
#include "builtin/lists.h"

class Experimental : public Unit {
public:
	Experimental(Runtime &runtime) : Unit(runtime) {
	}

	void initialize() {
		add("N",
		    Attributes::None, {
				    builtin<2>([] (
						    const BaseExpressionRef &expr,
						    const BaseExpressionRef &n,
						    const Evaluation &evaluation) {

					    if (n->type() != MachineIntegerType) {
						    return BaseExpressionRef();
					    }

					    const SymbolicForm form = expr->symbolic_form();
					    if (form.is_null()) {
						    return BaseExpressionRef();
					    }

					    return Heap::BigReal(
							    form,
							    Precision(double(static_cast<const MachineInteger*>(n.get())->value)));
				    })
		    });

		add("Expand",
		    Attributes::None, {
				    builtin<1>([] (
						    const BaseExpressionRef &expr,
						    const Evaluation &evaluation) {

					    return expr->expand(evaluation);
				    })
		    });


		add("Timing",
		    Attributes::HoldAll, {
				    builtin<1>(
						    [](const BaseExpressionRef &expr, const Evaluation &evaluation) {
							    const auto &List = evaluation.List;
							    const auto start_time = std::chrono::steady_clock::now();
							    const auto evaluated = expr->evaluate(evaluation);
							    const auto end_time = std::chrono::steady_clock::now();
							    const auto microseconds = std::chrono::duration_cast<
									    std::chrono::microseconds>(end_time - start_time).count();
							    return expression(List, {
									    from_primitive(double(microseconds) / 1000000.0), evaluated});
						    }
				    )
		    });
	}
};

void python_test(const char *input) {
    Runtime runtime;

    auto expr = runtime.parse(input);

    Evaluation evaluation(runtime.definitions(), true);
    std::cout << expr << std::endl;
    std::cout << "evaluating...\n";
    BaseExpressionRef evaluated = evaluation.evaluate(expr);
    std::cout << "done\n";
    std::cout << evaluated << std::endl;
}

/*void pattern_test() {
    Definitions definitions;

    auto x = definitions.new_symbol("System`x");

    auto patt = expression(definitions.lookup("System`Pattern"), {
        x, expression(definitions.lookup("System`Blank"), {})
    });

    Match m1 = match(patt, from_primitive(7LL), definitions);
    std::cout << m1 << std::endl;

    patt = expression(definitions.lookup("System`Pattern"), {
        x, expression(definitions.lookup("System`BlankSequence"), {})
    });

    auto some_expr = expression(definitions.lookup("System`Sequence"), {
		from_primitive(1LL), from_primitive(3LL)});

    Match m2 = match(patt, some_expr, definitions);
    std::cout << m2 << std::endl;
}*/

void mini_console() {
    Runtime runtime;

	Experimental(runtime).initialize();

	Builtin::Arithmetic(runtime).initialize();
	Builtin::Assignment(runtime).initialize();
	Builtin::Control(runtime).initialize();
	Builtin::Functional(runtime).initialize();
	Builtin::Lists(runtime).initialize();

    std::cout << ">> ";
    for (std::string line; std::getline(std::cin, line);) {
	    if (line.empty()) {
		    break;
	    }

	    try {
		    auto expr = runtime.parse(line.c_str());

		    Evaluation evaluation(runtime.definitions(), true);
		    BaseExpressionRef evaluated = evaluation.evaluate(expr);
		    std::cout << evaluated << std::endl;
	    } catch (parse_exception) {
		    std::cout << ": " << line << " could not be parsed." << std::endl;
	    }

	    std::cout << ">> ";
	    std::cout.flush();
    }
}

int main() {
    Heap::init();
	EvaluateDispatch::init();
	InstantiateSymbolicForm::init();

    python::Context context;

    mini_console();

    return 0;
}
