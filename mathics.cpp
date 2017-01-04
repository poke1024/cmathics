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
#include "core/string.h"
#include "core/builtin.h"
#include "core/evaluate.h"
#include "core/parser.h"
#include "core/runtime.h"


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

	const SymbolRef Line = runtime.definitions().symbols().StateLine;

	Line->own_value = Pool::MachineInteger(1);
    std::cout << "In[" << 1 << "]:= ";
    for (std::string line; std::getline(std::cin, line);) {
	    if (line.empty()) {
		    break;
	    }

	    try {
		    const auto expr = runtime.parse(line.c_str());

		    Evaluation evaluation(runtime.definitions(), true);
		    BaseExpressionRef evaluated = evaluation.evaluate(expr);

            const BaseExpressionRef line_number = Line->own_value;
			assert(line_number);
		    std::cout << "Out[" << line_number->fullform() << "]= " << evaluated << std::endl;
	    } catch (parse_exception) {
		    std::cout << ": " << line << " could not be parsed." << std::endl;
	    }

		// FIXME use increment().
        const BaseExpressionRef line_number = Line->own_value;
		assert(line_number && line_number->type() == MachineIntegerType);
        const BaseExpressionRef new_line_number = Pool::MachineInteger(
			static_cast<const MachineInteger*>(line_number.get())->value + 1);
        Line->own_value = new_line_number;

		std::cout << std::endl;
		std::cout << "In[" << new_line_number->fullform() << "]:= ";
	    std::cout.flush();
    }
}

int main() {
    Runtime::init();

    mini_console();

    return 0;
}
