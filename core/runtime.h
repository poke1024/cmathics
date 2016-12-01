#ifndef CMATHICS_RUNTIME_H
#define CMATHICS_RUNTIME_H

#include "types.h"
#include "builtin.h"
#include "parser.h"

class RulesBuilder {
private:
    const SymbolRef m_symbol;
    Definitions &m_definitions;

public:
    inline RulesBuilder(const SymbolRef &symbol, Definitions &definitions) :
        m_symbol(symbol), m_definitions(definitions) {
    }

    template<int N, typename F>
    void builtin(F func) {
        const auto rule = make_builtin_rule<N, F>(func);
        m_symbol->add_rule(rule(m_symbol, m_definitions));
    }
};

typedef std::function<void(RulesBuilder&, const SymbolRef)> SpecifyRules;

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

	void add(
		const char *name,
		Attributes attributes,
        SpecifyRules &rules);

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

	inline void add(
		const char *name,
		Attributes attributes,
        SpecifyRules rules) const {

		m_runtime.add(name, attributes, rules);
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
