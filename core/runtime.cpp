#include "runtime.h"

#include "builtin/arithmetic.h"
#include "builtin/assignment.h"
#include "builtin/control.h"
#include "builtin/exptrig.h"
#include "builtin/functional.h"
#include "builtin/lists.h"
#include "builtin/strings.h"
#include "builtin/structure.h"
#include "builtin/numbertheory.h"

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

                    const SymbolicFormRef form = symbolic_form(expr);
                    if (!form || form->is_none()) {
                        return BaseExpressionRef();
                    }

                    return Pool::BigReal(
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
                            return expression(List, from_primitive(double(microseconds) / 1000000.0), evaluated);
                        }
                    )
            });
    }
};

Runtime *Runtime::s_instance = nullptr;

void Runtime::init() {
    Pool::init();
    EvaluateDispatch::init();
    InstantiateSymbolicForm::init();
}

Runtime::Runtime() : _parser(_definitions) {
    const SymbolRef General = _definitions.symbols().General;

    General->add_message("normal", "Nonatomic expression expected.", _definitions);
    General->add_message("iterb", "Iterator does not have appropriate bounds.", _definitions);
	General->add_message("level", "Level specification `1` is not of the form n, {n}, or {m, n}.", _definitions);
    General->add_message("optx", "Unknown option `1` in `2`.", _definitions);
    General->add_message("string", "String expected.", _definitions);

    Experimental(*this).initialize();

    Builtins::Arithmetic(*this).initialize();
    Builtins::Assignment(*this).initialize();
    Builtins::Control(*this).initialize();
    Builtins::ExpTrig(*this).initialize();
    Builtins::Functional(*this).initialize();
    Builtins::Lists(*this).initialize();
    Builtins::Strings(*this).initialize();
    Builtins::Structure(*this).initialize();
    Builtins::NumberTheory(*this).initialize();

    assert(s_instance == nullptr);
    s_instance = this;
}

Runtime::~Runtime() {
    s_instance = nullptr;
}

Runtime *Runtime::get() {
    assert(s_instance);
    return s_instance;
}

void Runtime::add(
    const char *name,
    Attributes attributes,
    const std::initializer_list<NewRuleRef> &rules) {

    const std::string full_down = std::string("System`") + name;
    const SymbolRef symbol = _definitions.lookup(full_down.c_str());
    symbol->set_attributes(attributes);
    for (const NewRuleRef &new_rule : rules) {
        symbol->add_rule(new_rule(symbol, _definitions));
    }
}
