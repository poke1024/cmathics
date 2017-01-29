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

class Output {
public:
    virtual void write(
        const char *name,
        const char *tag,
        std::string &&s) = 0;
};

using OutputRef = std::shared_ptr<Output>;

class DefaultOutput : public Output {
public:
    virtual void write(
        const char *name,
        const char *tag,
        std::string &&s) {

        std::cout << name << "::" << tag << ": " << s << std::endl;
    }
};

class NoOutput : public Output {
public:
    virtual void write(
        const char *name,
        const char *tag,
        std::string &&s) {
    }
};

class TestOutput : public Output {
private:
    std::list<std::string> m_output;

public:
    virtual void write(
        const char *name,
        const char *tag,
        std::string &&s) {

        // ignore name and tag
        m_output.emplace_back(s);
    }

    void clear() {
        m_output.clear();
    }

    bool empty() {
        return m_output.empty();
    }

    bool test_empty() {
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

    bool test_line(const std::string &expected, bool fail_expected) {
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
};

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

    const BaseExpressionRef zero;
    const BaseExpressionRef one;
    const BaseExpressionRef minus_one;

    Evaluation(
        const OutputRef &output,
        Definitions &definitions,
        bool new_catch_interrupts);

    BaseExpressionRef evaluate(BaseExpressionRef expression);

    template<typename... Args>
    void message(const SymbolRef &name, const char *tag, const Args&... args) const;

    void sym_engine_exception(const SymEngine::SymEngineException &e) const;
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
        return unsafe_symbolic_form(item);
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
            std::cout << expr->format(
		        m_evaluation.StandardForm, m_evaluation);
        } else {
            std::cout << "IDENTITY";
        }
        return *this;
    }
};

#endif
