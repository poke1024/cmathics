#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "expression.h"
#include "definitions.h"
#include "evaluation.h"
#include "pattern.h"

Evaluation::Evaluation(Definitions &new_definitions, bool new_catch_interrupts) : definitions(new_definitions) {
    recursion_depth = 0;
    timeout = false;
    stopped = false;
    catch_interrupts = new_catch_interrupts;
    interrupt = NoInterrupt;
    out = NULL;
}


void send_message(Evaluation* evaluation, Symbol* symbol, char* tag) {
}


BaseExpressionRef Evaluation::evaluate(BaseExpressionRef expr) {
    // int64_t line_no;

    // TODO $HistoryLength

    // TODO Apply $Pre

    // TODO In[$Line]
    // line_no = get_line_no(evaluation);

    // perform evaluation
    auto evaluated = expr->evaluate(expr, *this);
	if (evaluated) {
		expr = evaluated;
	}

    // TODO $Post

    // TODO Out[$Line]
    // line_no = get_line_no(evaluation); // might have changed

    // TODO $PrePrint

    // TODO MessageList and $MessageList

    // TODO out callback

    // Increment $Line
    // set_line_no(evaluation, line_no + 1);

    // TODO clear aborts

    return expr;
}
