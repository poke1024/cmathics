// @formatter:off

#pragma once

#include <stdbool.h>
#include "definitions.h"
#include "core/atoms/string.h"

typedef enum {
    PrintType, MessageType
} OutType;

typedef struct Out{
    OutType type;
    struct Out* next;
} Out;

typedef struct {
    Out base;
    char* text;
} PrintOut;

typedef struct {
    Out base;
    Symbol* symbol;
    char* tag;
    char* text;
} MessageOut;

typedef enum {
    NoInterrupt, AbortInterrupt, TimeoutInterrupt, ReturnInterrupt, BreakInterrupt, ContinueInterrupt
} EvaluationInterrupt;

class Definitions;

inline std::string message_placeholder(size_t index) {
    std::ostringstream s;
    s << "`";
    s << index;
    s << "`";
    return s.str();
}

#include "output.h"
#include "numberform.h"

class Evaluation : public Symbols {
public:
    Definitions &definitions;
    int64_t recursion_depth;
    bool timeout;
    bool stopped;

    mutable std::mutex m_output_mutex;
    OutputRef m_output;

    bool catch_interrupts;
    EvaluationInterrupt interrupt;
    Out* out;
    mutable MutableBaseExpressionRef predetermined_out;
	mutable bool parallelize = false;

    Evaluation(
        const OutputRef &output,
        Definitions &definitions,
        bool new_catch_interrupts);

    BaseExpressionRef evaluate(BaseExpressionRef expression);

    template<typename... Args>
    void message(const SymbolRef &name, const char *tag, const Args&... args) const;

    void sym_engine_exception(const SymEngine::SymEngineException &e) const;

    std::string format_output(const BaseExpressionRef &expr) const;

    void print_out(const ExpressionRef &expr) const;
};

inline SymbolRef String::option_symbol(const Evaluation &evaluation) const {
	ConstSymbolRef symbol = m_option_symbol;
	if (!symbol) {
		const std::string fullname = std::string("System`") + utf8();
		const SymbolRef new_symbol = evaluation.definitions.lookup_no_create(fullname.c_str());
		m_option_symbol = new_symbol;
        return new_symbol;
	} else {
        return symbol;
    }
}

template<typename T>
inline SymbolicFormRef symbolic_form(const T &item, const Evaluation &evaluation) {
    try {
        return unsafe_symbolic_form(item, evaluation);
    } catch (const SymEngine::SymEngineException &e) {
        evaluation.sym_engine_exception(e);
        return SymbolicFormRef();
    }
}

class DebugOutput {
private:
    const Evaluation &m_evaluation;

public:
    DebugOutput(const Evaluation &evaluation) : m_evaluation(evaluation) {
    }

    inline DebugOutput &operator<<(const char *s) {
        std::cout << s;
        return *this;
    }

    inline DebugOutput &operator<<(const std::string &s) {
        std::cout << s;
        return *this;
    }

    inline DebugOutput &operator<<(const BaseExpressionRef &expr) {
        if (expr) {
            std::cout << m_evaluation.format_output(expr);
        } else {
            std::cout << "IDENTITY";
        }
        return *this;
    }
};

inline bool BaseExpression::has_form(
	SymbolName head,
    size_t n_leaves) const {

    if (!is_expression()) {
        return false;
    }
    const Expression * const expr = as_expression();
    if (expr->head()->symbol() == head && expr->size() == n_leaves) {
        return true;
    } else {
        return false;
    }
}

inline void BaseExpression::set_no_symbolic_form(const Evaluation &evaluation) const {
	m_symbolic_form.ensure([&evaluation] () {
		return evaluation.definitions.no_symbolic_form;
	});
}

template<>
inline SymbolicFormRef unsafe_symbolic_form<const ExpressionPtr&>(
	const ExpressionPtr& expr, const Evaluation &evaluation) {

	// caller must handle SymEngine::SymEngineException; non-core code should always
	// call symbolic_form(item, evaluation).

	return SymbolicFormRef(expr->m_symbolic_form.ensure([&evaluation] () {
		return evaluation.definitions.no_symbolic_form;
	}));
}

#include "expression/implementation.h"
#include "evaluation.tcc"
#include "cache.tcc"

