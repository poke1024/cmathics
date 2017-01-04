#ifndef CMATHICS_RUNTIME_H
#define CMATHICS_RUNTIME_H

#include "types.h"
#include "builtin.h"
#include "parser.h"
#include "matcher.h"

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

		auto command = ConstSharedPtr<T>(new T(*this, symbol, _definitions));
		command->build(*this);
    }
};

class Builtin : public Shared<Builtin, SharedHeap> {
private:
	template<typename T>
	auto shared_from_this() {
		return ConstSharedPtr<T>(static_cast<T*>(this));
	}

protected:
	Runtime &m_runtime;
	const SymbolRef m_symbol;

    template<typename T>
    using F1 = BaseExpressionRef (T::*) (
		BaseExpressionPtr,
        const Evaluation &);

	template<typename T>
	using B1 = bool (T::*) (
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

	template<typename T>
	using FN = BaseExpressionRef (T::*) (
		const BaseExpressionRef *,
		size_t,
		const Evaluation &);

	template<typename T, typename Options>
	using OptionsF2 = BaseExpressionRef (T::*) (
		BaseExpressionPtr,
		BaseExpressionPtr,
		const Options &options,
		const Evaluation &);

    template<typename T, typename Options>
    using OptionsF3 = BaseExpressionRef (T::*) (
        BaseExpressionPtr,
        BaseExpressionPtr,
        BaseExpressionPtr,
        const Options &options,
        const Evaluation &);

	template<typename T>
	inline void builtin(F1<T> fptr) {
        const auto self = shared_from_this<T>();

		auto func = [self, fptr] (
			BaseExpressionPtr a,
			const Evaluation &evaluation) {

			auto p = self.get();
			return (p->*fptr)(a, evaluation);
		};

		m_symbol->add_rule(new BuiltinRule<1, decltype(func)>(
			m_symbol,
			m_runtime.definitions(),
			func));
	}

	template<typename T>
	inline void builtin(B1<T> fptr) {
		const auto self = shared_from_this<T>();

		auto func = [self, fptr] (
			BaseExpressionPtr a,
			const Evaluation &evaluation) {

			auto p = self.get();
			if ((p->*fptr)(a, evaluation)) {
				return evaluation.True;
			} else {
				return evaluation.False;
			}
		};

		m_symbol->add_rule(new BuiltinRule<1, decltype(func)>(
			m_symbol,
			m_runtime.definitions(),
			func));
	}

    template<typename T>
    inline void builtin(F2<T> fptr) {
		const auto self = shared_from_this<T>();

	    auto func = [self, fptr] (
			BaseExpressionPtr a,
			BaseExpressionPtr b,
		    const Evaluation &evaluation) {

		    auto p = self.get();
		    return (p->*fptr)(a, b, evaluation);
	    };

	    m_symbol->add_rule(new BuiltinRule<2, decltype(func)>(
		    m_symbol,
		    m_runtime.definitions(),
		    func));
    }

    template<typename T>
    inline void builtin(F3<T> fptr) {
		const auto self = shared_from_this<T>();

	    auto func = [self, fptr] (
		    BaseExpressionPtr a,
		    BaseExpressionPtr b,
		    BaseExpressionPtr c,
		    const Evaluation &evaluation) {

		    auto p = self.get();
		    return (p->*fptr)(a, b, c, evaluation);
	    };

	    m_symbol->add_rule(new BuiltinRule<3, decltype(func)>(
		    m_symbol,
		    m_runtime.definitions(),
            func));
    }

	template<typename T>
	inline void builtin(FN<T> fptr) {
		const auto self = shared_from_this<T>();

		auto func = [self, fptr] (
			const BaseExpressionRef *leaves,
			size_t n,
			const Evaluation &evaluation) {

			auto p = self.get();
			return (p->*fptr)(leaves, n, evaluation);
		};

		m_symbol->add_rule(new VariadicBuiltinRule<0, decltype(func)>(
			m_symbol,
			m_runtime.definitions(),
			func));
	}

	template<typename T, typename Options>
	inline void builtin(const OptionsInitializerList &options, OptionsF2<T, Options> fptr) {
		const auto self = shared_from_this<T>();

		auto func = [self, fptr] (
			BaseExpressionPtr a,
			BaseExpressionPtr b,
			const Options &options,
			const Evaluation &evaluation) {

			auto p = self.get();
			return (p->*fptr)(a, b, options, evaluation);
		};

		m_symbol->add_rule(new OptionsBuiltinRule<2, Options, decltype(func)>(
			m_symbol,
			m_runtime.definitions(),
			options,
			func));
	}

    template<typename T, typename Options>
    inline void builtin(const OptionsInitializerList &options, OptionsF3<T, Options> fptr) {
		const auto self = shared_from_this<T>();

        auto func = [self, fptr] (
            BaseExpressionPtr a,
            BaseExpressionPtr b,
            BaseExpressionPtr c,
            const Options &options,
            const Evaluation &evaluation) {

            auto p = self.get();
            return (p->*fptr)(a, b, c, options, evaluation);
        };

        m_symbol->add_rule(new OptionsBuiltinRule<3, Options, decltype(func)>(
            m_symbol,
            m_runtime.definitions(),
            options,
            func));
    }

	inline void down(const char *pattern, const char *into) {
		m_symbol->add_rule(new DownRule(
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

typedef ConstSharedPtr<Builtin> BuiltinRef;

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

	inline NewRuleRef down(const char *pattern, const char *into) const {
		return make_down_rule(m_runtime.parse(pattern), m_runtime.parse(into));
	}

public:
	Unit(Runtime &runtime) : m_runtime(runtime) {
	}
};

#endif //CMATHICS_RUNTIME_H
