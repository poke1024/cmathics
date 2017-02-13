#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "expression.h"
#include "definitions.h"
#include "evaluation.h"
#include "pattern.h"

Evaluation::Evaluation(
    const OutputRef &output,
    Definitions &new_definitions,
    bool new_catch_interrupts) :

	Symbols(new_definitions.symbols()),

    m_output(output),
	definitions(new_definitions),
	zero(Pool::MachineInteger(0)),
	one(Pool::MachineInteger(1)),
	minus_one(Pool::MachineInteger(-1)),
    empty_list(expression(this->List)),
    number_form(*this) {

    recursion_depth = 0;
    timeout = false;
    stopped = false;
    catch_interrupts = new_catch_interrupts;
    interrupt = NoInterrupt;
    out = NULL;
}

BaseExpressionRef Evaluation::evaluate(BaseExpressionRef expr) {
    // int64_t line_no;

    // TODO $HistoryLength

    // TODO Apply $Pre

    // TODO In[$Line]
    // line_no = get_line_no(evaluation);

    // perform evaluation
    const auto evaluated = coalesce(expr->evaluate(*this), expr);

    // TODO $Post

    // TODO Out[$Line]
    // line_no = get_line_no(evaluation); // might have changed

    // TODO $PrePrint

    // TODO MessageList and $MessageList

    // TODO out callback

    // Increment $Line
    // set_line_no(evaluation, line_no + 1);

    // TODO clear aborts

    return evaluated;
}

void Evaluation::sym_engine_exception(const SymEngine::SymEngineException &e) const {
    const std::string what(e.what());

    if (what == "Indeterminate Expression: `0 * Infty` encountered") {
        message(definitions.lookup("General"), "indet", Pool::String(std::string("0 Infinity")));
    } else {
        std::cerr << "SymEngine error: " << what << std::endl;
    }
}

std::string Evaluation::format_output(const BaseExpressionRef &expr) const {
	return expr->make_boxes(OutputForm, *this)->boxes_to_text(*this);
}

void Evaluation::print_out(const ExpressionRef &expr) const {
    std::string text = expr->make_boxes(this->OutputForm, *this)->boxes_to_text(*this);
    std::cout << text << std::endl;
}
