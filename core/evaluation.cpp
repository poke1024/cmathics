#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "expression.h"
#include "definitions.h"
#include "evaluation.h"
#include "pattern.h"

Evaluation::Evaluation(Definitions &new_definitions, bool new_catch_interrupts) :
	Symbols(new_definitions.symbols()),
	definitions(new_definitions) {

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

void Evaluation::message(const SymbolRef &name, const char *tag) const {
    const auto &symbols = definitions.symbols();

    const ExpressionRef message = expression(
        symbols.MessageName, name, Heap::String(tag));
    StringRef text = name->lookup_message(message.get(), *this);

    if (!text) {
        const ExpressionRef general_message = expression(
            symbols.MessageName, symbols.General, Heap::String(tag));

        text = symbols.General->lookup_message(general_message.get(), *this);
    }

    if (text) {
        std::cout << name->short_name() << "::" << tag << ": " << text->c_str() << std::endl;
    }
}

void Evaluation::message(const SymbolRef &name, const char *tag, const BaseExpressionRef &arg1) const {
}

void Evaluation::message(const SymbolRef &name, const char *tag, const BaseExpressionRef &arg1, const BaseExpressionRef &arg2) const {
}
