#include <stdbool.h>
#include <stdlib.h>

#include "types.h"
#include "core/expression/implementation.h"
#include "definitions.h"
#include "evaluation.h"
#include "core/pattern/arguments.h"

Evaluation::Evaluation(
    const OutputRef &output,
    Definitions &new_definitions,
    bool new_catch_interrupts) :

	Symbols(new_definitions.symbols()),
    zero(new_definitions.zero.get()),

    m_output(output),
	definitions(new_definitions) {

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
        message(definitions.lookup("General"), "indet", String::construct(std::string("0 Infinity")));
    } else {
        std::cerr << "SymEngine error: " << what << std::endl;
    }
}

std::string Evaluation::format_output(const BaseExpressionRef &expr) const {
    StyleBoxOptions options;
    return expr->make_boxes(OutputForm, *this)->boxes_to_text(options, *this);
}

void Evaluation::print_out(const ExpressionRef &expr) const {
    StyleBoxOptions options;
    std::string text =  expr->make_boxes(OutputForm, *this)->boxes_to_text(options, *this);
    std::cout << text << std::endl;
}

bool TestOutput::test_empty() {
    if (!m_output.empty()) {
        for (const std::string &s : m_output) {
            std::cout << "unexpected output: " << s << std::endl;
        }
        m_output.clear();
        return false;
    } else {
        return true;
    }
}

bool TestOutput::test_line(const std::string &expected, bool fail_expected) {
    bool fail = false;
    std::string actual;

    if (m_output.size() >= 1) {
        actual = m_output.front();
        if (expected != actual) {
            fail = true;
        } else {
            m_output.erase(m_output.begin());
        }
    } else {
        fail = true;
        actual = "(nothing)";
    }

    if (fail && !fail_expected) {
        std::cout << "output mismatch: " << std::endl;
        std::cout << "expected: " << expected << std::endl;
        std::cout << "actual: " << actual << std::endl;
        return false;
    } else {
        return true;
    }
}