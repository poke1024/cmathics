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

	inline const Symbols &symbols() const {
		return _definitions.symbols();
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
		command->build(*this);
    }
};

class Builtin : public std::enable_shared_from_this<Builtin> {
protected:
	Runtime &m_runtime;
	const SymbolRef m_symbol;

    template<typename T>
    using F1 = BaseExpressionRef (T::*) (
		BaseExpressionPtr,
        const Evaluation &);

    template<typename T>
    using F2 = BaseExpressionRef (T::*) (
		BaseExpressionPtr,
		BaseExpressionPtr,
        const Evaluation &);

    template<typename T>
    using F3 = BaseExpressionRef (T::*) (
		BaseExpressionPtr,
		BaseExpressionPtr,
		BaseExpressionPtr,
        const Evaluation &);

	template<typename T, typename Options>
	using OptionsF2 = BaseExpressionRef (T::*) (
		BaseExpressionPtr,
		BaseExpressionPtr,
		const Options &options,
		const Evaluation &);

	template<typename T>
	inline void builtin(F1<T> fptr) {
        const auto self = std::static_pointer_cast<T>(shared_from_this());

		auto func = [self, fptr] (
			BaseExpressionPtr a,
			const Evaluation &evaluation) {

			auto p = self.get();
			return (p->*fptr)(a, evaluation);
		};

		m_symbol->add_rule(std::make_shared<BuiltinRule<1, decltype(func)>>(
			m_symbol,
			m_runtime.definitions(),
			func));
	}

    template<typename T>
    inline void builtin(F2<T> fptr) {
        const auto self = std::static_pointer_cast<T>(shared_from_this());

	    auto func = [self, fptr] (
		    BaseExpressionPtr a,
		    BaseExpressionPtr b,
		    const Evaluation &evaluation) {

		    auto p = self.get();
		    return (p->*fptr)(a, b, evaluation);
	    };

	    m_symbol->add_rule(std::make_shared<BuiltinRule<2, decltype(func)>>(
		    m_symbol,
		    m_runtime.definitions(),
		    func));
    }

    template<typename T>
    inline void builtin(F3<T> fptr) {
        const auto self = std::static_pointer_cast<T>(shared_from_this());

	    auto func = [self, fptr] (
		    BaseExpressionPtr a,
		    BaseExpressionPtr b,
		    BaseExpressionPtr c,
		    const Evaluation &evaluation) {

		    auto p = self.get();
		    return (p->*fptr)(a, b, c, evaluation);
	    };

	    m_symbol->add_rule(std::make_shared<BuiltinRule<3, decltype(func)>>(
		    m_symbol,
		    m_runtime.definitions(),
            func));
    }

	template<typename T, typename Options>
	inline void builtin(const OptionsInitializerList &options, OptionsF2<T, Options> fptr) {
		const auto self = std::static_pointer_cast<T>(shared_from_this());

		auto func = [self, fptr] (
			BaseExpressionPtr a,
			BaseExpressionPtr b,
			const Options &options,
			const Evaluation &evaluation) {

			auto p = self.get();
			return (p->*fptr)(a, b, options, evaluation);
		};

		m_symbol->add_rule(std::make_shared<OptionsBuiltinRule<2, Options, decltype(func)>>(
			m_symbol,
			m_runtime.definitions(),
			options,
			func));
	}

	inline void rewrite(const char *pattern, const char *into) {
		m_symbol->add_rule(std::make_shared<RewriteRule>(
			m_runtime.parse(pattern), m_runtime.parse(into)));
	}

    inline void message(const char *tag, const char *text) {
        m_symbol->add_message(tag, text, m_runtime.definitions());
    }

public:
    static constexpr auto attributes = Attributes::None;

    Builtin(Runtime &runtime, const SymbolRef &symbol, Definitions &definitions) :
		m_runtime(runtime), m_symbol(symbol) {
	}
};

typedef std::shared_ptr<Builtin> BuiltinRef;

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
