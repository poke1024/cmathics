#include "runtime.h"

#include "builtin/arithmetic.h"
#include "builtin/assignment.h"
#include "builtin/comparison.h"
#include "builtin/control.h"
#include "builtin/evaluation.h"
#include "builtin/exptrig.h"
#include "builtin/functional.h"
#include "builtin/inout.h"
#include "builtin/lists.h"
#include "builtin/options.h"
#include "builtin/strings.h"
#include "builtin/structure.h"
#include "builtin/numbertheory.h"

#include "concurrent/parallel.h"

#if MAKE_UNIT_TEST
const char *Builtin::docs = "";
#endif

class N : public Builtin {
public:
    static constexpr const char *name = "N";

    static constexpr const char *docs = R"(
    )";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin("N[expr_]", "N[expr, MachinePrecision]");
        builtin(&N::apply);
    }

    inline BaseExpressionRef apply(
        const BaseExpressionPtr expr,
        const BaseExpressionPtr n,
        const Evaluation &evaluation) {

        const SymbolicFormRef form = symbolic_form(expr, evaluation);
        if (!form || form->is_none()) {
            return BaseExpressionRef();
        }

        try {
            if (n->extended_type() == SymbolMachinePrecision) {
                return Pool::MachineReal(form);
            } else {
                double p;

                switch (n->type()) {
                    case MachineIntegerType:
                        p = static_cast<const MachineInteger*>(n)->value;
                        break;
                    case MachineRealType:
                        p = static_cast<const MachineReal*>(n)->value;
                        break;
                    default:
                        return BaseExpressionRef();
                }

                if (p <= Precision::machine_precision.decimals) {
                    return Pool::MachineReal(form);
                } else {
                    return Pool::BigReal(form, Precision(p));
                }
            }
        } catch (const SymEngine::SymEngineException &e) {
            evaluation.sym_engine_exception(e);
            return BaseExpressionRef();
        }
    }
};

class Experimental : public Unit {
public:
    Experimental(Runtime &runtime) : Unit(runtime) {
    }

    void initialize() {
        add<N>();

        add("Expand",
            Attributes::None, {
                builtin<1>([] (
                    ExpressionPtr,
                    const BaseExpressionRef &expr,
                    const Evaluation &evaluation) {

                    return expr->expand(evaluation);
                })
            });


        add("Timing",
            Attributes::HoldAll, {
                builtin<1>(
                    [](ExpressionPtr, const BaseExpressionRef &expr, const Evaluation &evaluation) {
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

        add("Parallelize",
            Attributes::HoldAll, {
            builtin<1>(
                [](ExpressionPtr, const BaseExpressionRef &expr, const Evaluation &evaluation) {
                    const bool old_parallelize = evaluation.parallelize;
                    evaluation.parallelize = true;
                    try {
	                    const auto evaluated = expr->evaluate(evaluation);
	                    evaluation.parallelize = old_parallelize;
	                    return evaluated;
                    } catch(...) {
	                    evaluation.parallelize = old_parallelize;
	                    throw;
                    }
                }
            )
        });
    }
};

Runtime *Runtime::s_instance = nullptr;

void Runtime::init() {
    Pool::init();
    EvaluateDispatch::init();
    //InstantiateSymbolicForm::init();
    Parallel::init();
}

Runtime::Runtime() : _parser(_definitions) {
    const SymbolRef General = _definitions.symbols().General;

    General->add_message("normal", "Nonatomic expression expected.", _definitions);
    General->add_message("iterb", "Iterator does not have appropriate bounds.", _definitions);
	General->add_message("level", "Level specification `1` is not of the form n, {n}, or {m, n}.", _definitions);
    General->add_message("optx", "Unknown option `1` in `2`.", _definitions);
    General->add_message("string", "String expected.", _definitions);
    General->add_message("indet", "Indeterminate expression `1` encountered.", _definitions);

    Experimental(*this).initialize();

    Builtins::Arithmetic(*this).initialize();
    Builtins::Assignment(*this).initialize();
    Builtins::Comparison(*this).initialize();
    Builtins::Control(*this).initialize();
    Builtins::Evaluation(*this).initialize();
    Builtins::ExpTrig(*this).initialize();
    Builtins::Functional(*this).initialize();
	Builtins::InOut(*this).initialize();
    Builtins::Options(*this).initialize();
    Builtins::Lists(*this).initialize();
    Builtins::Strings(*this).initialize();
    Builtins::Structure(*this).initialize();
    Builtins::NumberTheory(*this).initialize();

    assert(s_instance == nullptr);
    s_instance = this;
}

Runtime::~Runtime() {
	Parallel::shutdown();
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
    symbol->state().set_attributes(attributes);
    for (const NewRuleRef &new_rule : rules) {
        symbol->state().add_rule(new_rule(symbol, _definitions));
    }
}

#if MAKE_UNIT_TEST
// trim is taken from http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
inline std::string trim(const std::string &s) {
	auto wsfront=std::find_if_not(s.begin(),s.end(),[](int c){return std::isspace(c);});
	auto wsback=std::find_if_not(s.rbegin(),s.rend(),[](int c){return std::isspace(c);}).base();
	return (wsback<=wsfront ? std::string() : std::string(wsfront,wsback));
}

void Runtime::run_tests() {
	// also see mathics/test.py

	size_t n_tests = 0;

	for (const auto &record : m_docs) {
		std::stringstream stream(record.second);
		std::string line;
		while(std::getline(stream, line, '\n')) {
			if (line.compare(0, 2, ">>") == 0) {
				n_tests++;
			}
		}
	}

	const int n_digits = std::ceil(std::log10(n_tests));

	size_t index = 1;
	size_t fail_count = 0;

    const auto output = std::make_shared<TestOutput>();
    const auto no_output = std::make_shared<NoOutput>();

	for (const auto &record : m_docs) {
		Evaluation evaluation(output, _definitions, false);
        Evaluation dummy_evaluation(no_output, _definitions, false);
        const auto fullform = evaluation.FullForm;

		std::stringstream stream(record.second);
		UnsafeBaseExpressionRef result;

		std::string line;
		bool fail_expected = false;
		while(std::getline(stream, line, '\n')) {
			line = trim(line);
			if (line.compare(0, 2, ">>") == 0 || line.compare(0, 2, "#>") == 0) {
				const std::string command_str(trim(line.substr(2)));
				std::cout << std::setw(n_digits) << index++ << ". TEST " << command_str << std::endl;

                if (!output->test_empty()) {
                    fail_count += 1;
                }
				result = _parser.parse(command_str.c_str())->evaluate_or_copy(evaluation);

				fail_expected = line.compare(0, 2, "#>") == 0;
			} else if (line.compare(0, 1, "=") == 0) {
                const std::string trimmed_line(trim(line.substr(1)));

                if (trimmed_line.length() > 2 &&
	                trimmed_line[0] == '-' &&
	                trimmed_line[trimmed_line.length() - 1] == '-') {
                    // ignore stuff like -Graphics- for now
                } else {
                    if (result) {
                        auto wanted_str = trimmed_line;

                        const std::string result_str = evaluation.format_output(result);

                        if (wanted_str != result_str) {
                            if (!fail_expected) {
                                std::cout << "FAIL" << std::endl;
	                            std::cout << "Result: " << result_str << std::endl;
	                            std::cout << "Wanted: " << wanted_str << std::endl;
                                fail_count++;
                            }
                        }
                    } else {
                        std::cout << "FAIL" << std::endl;
                        std::cout << "undefined result" << std::endl;
                        fail_count++;
                    }
                }
            } else if (line.compare(0, 1, ":") == 0) {
                if (!output->test_line(trim(line.substr(1)), fail_expected)) {
                    fail_count++;
                }
			} else {
				// ignore
			}
		}
	}

    if (!output->test_empty()) {
        fail_count += 1;
    }

	if (fail_count == 0) {
	    std::cout << "TESTS OK" << std::endl;
	} else {
	    std::cout << fail_count << " TESTS FAILED!" << std::endl;
	}
}
#endif
