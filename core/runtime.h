#ifndef CMATHICS_RUNTIME_H
#define CMATHICS_RUNTIME_H

#include "types.h"
#include "builtin.h"
#include "parser.h"

class Runtime {
private:
	Definitions _definitions;
	Parser _parser;

public:
	Runtime();

	inline Definitions &definitions() {
		return _definitions;
	}

	inline Parser &parser() {
		return _parser;
	}

	BaseExpressionRef parse(const char *s) {
		return _parser.parse(s);
	}

	void add(
		const char *name,
		Attributes attributes,
		const std::initializer_list <NewRuleRef> &rules);

	template<typename T>
	void add() {
		const std::string full_down = std::string("System`") + T::name;
		const SymbolRef symbol = _definitions.lookup(full_down.c_str());
		symbol->set_attributes(T::attributes);

		auto command = std::make_shared<T>(*this, symbol, _definitions);
		command->init();
	}
};

class Builtin : public std::enable_shared_from_this<Builtin> {
protected:
	Runtime &m_runtime;
	const SymbolRef m_symbol;
	Definitions &m_definitions;

    template<typename T>
    using F1 = BaseExpressionRef (T::*) (
        const BaseExpressionRef &,
        const Evaluation &);

    template<typename T>
    using F2 = BaseExpressionRef (T::*) (
        const BaseExpressionRef &,
        const BaseExpressionRef &,
        const Evaluation &);

    template<typename T>
    using F3 = BaseExpressionRef (T::*) (
        const BaseExpressionRef &,
        const BaseExpressionRef &,
        const BaseExpressionRef &,
        const Evaluation &);

    template<typename T>
	inline void builtin(F1<T> fptr) {
        auto self = std::static_pointer_cast<T>(shared_from_this());
		const auto rule = make_builtin_rule<1>(
            [self, fptr] (
                const BaseExpressionRef &a,
                const Evaluation &evaluation) {

                auto p = self.get();
                return (p->*fptr)(a, evaluation);
            });
		m_symbol->add_rule(rule(m_symbol, m_definitions));
	}

    template<typename T>
    inline void builtin(F2<T> fptr) {
        auto self = std::static_pointer_cast<T>(shared_from_this());
        const auto rule = make_builtin_rule<2>(
                [self, fptr] (
                    const BaseExpressionRef &a,
                    const BaseExpressionRef &b,
                    const Evaluation &evaluation) {

                    auto p = self.get();
                    return (p->*fptr)(a, b, evaluation);
                });
        m_symbol->add_rule(rule(m_symbol, m_definitions));
    }

    template<typename T>
    inline void builtin(F3<T> fptr) {
        auto self = std::static_pointer_cast<T>(shared_from_this());
        const auto rule = make_builtin_rule<3>(
                [self, fptr] (
                    const BaseExpressionRef &a,
                    const BaseExpressionRef &b,
                    const BaseExpressionRef &c,
                    const Evaluation &evaluation) {

                    auto p = self.get();
                    return (p->*fptr)(a, b, c, evaluation);
                });
        m_symbol->add_rule(rule(m_symbol, m_definitions));
    }

	inline void rewrite(const char *pattern, const char *into) {
		const auto rule = make_rewrite_rule(m_runtime.parse(pattern), m_runtime.parse(into));
		m_symbol->add_rule(rule(m_symbol, m_definitions));
	}

public:
	Builtin(Runtime &runtime, const SymbolRef &symbol, Definitions &definitions) :
		m_runtime(runtime), m_symbol(symbol), m_definitions(definitions) {
	}
};

class Unit {
private:
	Runtime &m_runtime;

protected:
	inline void add(
		const char *name,
		Attributes attributes,
		const std::initializer_list<NewRuleRef> &rules) const {

		m_runtime.add(name, attributes, rules);
	}

	template<typename T>
	inline void add() const {
		m_runtime.add<T>();
	}

public:
	template<int N, typename F>
	inline NewRuleRef builtin(F func) const {
		return make_builtin_rule<N, F>(func);
	}

	template<int N>
	inline NewRuleRef pattern_matched_builtin(const char *pattern, typename BuiltinFunctionArguments<N>::type func) const {
		return make_pattern_matched_builtin_rule<N>(m_runtime.parse(pattern), func);
	}

	inline NewRuleRef rewrite(const char *pattern, const char *into) const {
		return make_rewrite_rule(m_runtime.parse(pattern), m_runtime.parse(into));
	}

public:
	Unit(Runtime &runtime) : m_runtime(runtime) {
	}
};

#endif //CMATHICS_RUNTIME_H
