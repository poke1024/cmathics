#ifndef EVALUATION_H
#define EVALUATION_H

#include <stdbool.h>
#include "definitions.h"
#include "string.h"

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

inline std::string message_text(
    std::string &&text,
    size_t index) {

    return text;
}

template<typename... Args>
std::string message_text(
    std::string &&text,
    size_t index,
    const BaseExpressionRef &arg,
    Args... args) {

    std::string new_text(text);
    const std::string placeholder(message_placeholder(index));

    const auto pos = new_text.find(placeholder);
    if (pos != std::string::npos) {
        new_text = new_text.replace(pos, placeholder.length(), arg->fullform());
    }

    return message_text(std::move(new_text), index + 1, args...);
}

class Evaluation : public Symbols {
public:
    Definitions &definitions;
    int64_t recursion_depth;
    bool timeout;
    bool stopped;
    // Output* output;
    bool catch_interrupts;
    EvaluationInterrupt interrupt;
    Out* out;
    mutable MutableBaseExpressionRef predetermined_out;

    const BaseExpressionRef zero;
    const BaseExpressionRef one;

    Evaluation(Definitions &definitions, bool new_catch_interrupts);

    BaseExpressionRef evaluate(BaseExpressionRef expression);

    template<typename... Args>
    void message(const SymbolRef &name, const char *tag, Args... args) const;
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

#endif
