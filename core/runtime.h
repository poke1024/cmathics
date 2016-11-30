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
	Runtime() : _parser(_definitions) {
	}

	inline Definitions &definitions() {
		return _definitions;
	}

	inline Parser &parser() {
		return _parser;
	}

	BaseExpressionRef parse(const char *s) {
		return _parser.parse(s);
	}

	void add(const char *name, Attributes attributes, const std::initializer_list <NewRuleRef> &rules) {
		const std::string full_down = std::string("System`") + name;
		const SymbolRef symbol = _definitions.lookup(full_down.c_str());
		symbol->set_attributes(attributes);
		for (const NewRuleRef &new_rule : rules) {
			symbol->add_rule(new_rule(symbol, _definitions));
		}
	}
};

class Unit {
private:
	Runtime &m_runtime;

protected:
	inline void add(const char *name, Attributes attributes, const std::initializer_list<NewRuleRef> &rules) const {
		m_runtime.add(name, attributes, rules);
	}

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
