// @formatter:off

#ifndef CMATHICS_RUNTIME_H
#define CMATHICS_RUNTIME_H

#include "types.h"
#include "builtin.h"
#include "parser.h"
#include "core/matcher/matcher.tcc"
#include "python.h"

using TestList = std::initializer_list<const char*[2]>;

class Runtime {
private:
	std::map<std::string, const char*> m_docs;

    python::Context _python_context;
	Definitions _definitions;
	Parser _parser;
	Evaluation _bootstrap_evaluation;

    static Runtime *s_instance;

public:
    static void init(); // call once

    static Runtime *get();

	Runtime();
    ~Runtime();

	inline Definitions &definitions() {
		return _definitions;
	}

	inline const Evaluation &evaluation() {
		return _bootstrap_evaluation;
	}

	inline Parser &parser() {
		return _parser;
	}

	inline const Symbols &symbols() const {
		return _definitions.symbols();
	}

    template<typename... Args>
	BaseExpressionRef parse(const char *format, const Args&... args) {
        UnsafeBaseExpressionRef result;
        STATIC_IF (sizeof...(args) == 0) {
    		result = _parser.parse(format);
        } STATIC_ELSE {
            std::string s(string_format(format, args...));
            result = _parser.parse(s.c_str());
        } STATIC_ENDIF
        return result;
	}

	void add(
		const char *name,
		Attributes attributes,
		const std::initializer_list<NewRuleRef> &rules);

	template<typename T>
	void add() {
		const std::string full_name = std::string("System`") + T::name;
		const SymbolRef symbol = _definitions.lookup(full_name.c_str());

		Attributes attributes = T::attributes;

		if (full_name != "System`MakeBoxes") {
			attributes = attributes + Attributes::Protected;
		}

		symbol->mutable_state().add_attributes(
            attributes, _bootstrap_evaluation);

        Runtime &runtime_ref = *this;
        const SymbolRef &symbol_ref = symbol;
        Definitions &definitions_ref = _definitions;

		ConstSharedPtr<T> command(new T(
            runtime_ref, symbol_ref, definitions_ref));
		command->build(runtime_ref);

#if MAKE_UNIT_TEST
		add_docs(T::name, T::docs);
#endif
    }

#if MAKE_UNIT_TEST
	void add_docs(
		const char *name,
		const char *docs) {

        m_docs[name] = docs;
	}

	void run_tests();
#endif
};

template<typename T>
const T &last(const T &x) {
    return x;
};

template<typename T, typename... Args>
const auto &last(const T &x, const Args&... args) {
    return last(args...);
};

struct EmptyExpression {
    const ExpressionPtr expr;
    EmptyExpression(ExpressionPtr e) : expr(e) {
    }
};

class Builtin : public HeapObject<Builtin> {
private:
    template<typename T>
    auto shared_from_this() {
        return ConstSharedPtr<T>(static_cast<T*>(this));
    }

	inline SymbolState &rule_owner(const BaseExpressionRef &lhs) const {
		// compare Python Mathics Builtin::contribute; we do a special
		// case for MakeBoxes to add down rules even though they are
		// specified as up rules.

		const auto &symbols = m_runtime.symbols();
		if (lhs->head(symbols) == symbols.MakeBoxes) {
			return symbols.MakeBoxes->mutable_state();
		} else {
			return m_symbol->mutable_state();
		}
	}

    template<typename T, typename M>
    class Bridge {
    protected:
        const ConstSharedPtr<T> m_self;
        const M m_method;

    public:
        inline Bridge(Builtin *instance, const M &method) :
            m_self(instance->shared_from_this<T>()), m_method(method) {
            }
    };

    template<typename T, typename M, int N>
    class ArgumentsBridge : public Bridge<T, M> {
    public:
        using Bridge<T, M>::Bridge;

        template<typename... Args>
        inline auto operator()(ExpressionPtr expr, const Args&... args) const {
            const auto p = this->m_self.get();
            return (p->*this->m_method)(args...);
        }
    };

    template<typename T, typename M, int N>
    class ExtendedArgumentsBridge : public Bridge<T, M> {
    public:
        using Bridge<T, M>::Bridge;

        template<typename... Args>
        inline auto operator()(ExpressionPtr expr, const Args&... args) const {
            const auto p = this->m_self.get();
            return (p->*this->m_method)(expr, args...);
        }
    };

    template<typename T, typename M>
    class ExtendedArgumentsBridge<T, M, 0> : public Bridge<T, M> {
    public:
        using Bridge<T, M>::Bridge;

        template<typename... Args>
        inline auto operator()(ExpressionPtr expr, const Args&... args) const {
            const auto p = this->m_self.get();
            return (p->*this->m_method)(EmptyExpression(expr), args...);
        }
    };

    template<typename T, typename M, int N>
    class BooleanBridge : public Bridge<T, M> {
    public:
        using Bridge<T, M>::Bridge;

        template<typename... Args>
        inline auto operator()(ExpressionPtr expr, const Args&... args) const {
            const auto p = this->m_self.get();
            const Evaluation &evaluation = last(args...);
			if ((p->*this->m_method)(args...)) {
				return evaluation.True;
			} else {
				return evaluation.False;
			}
        }
    };

protected:
	Runtime &m_runtime;
	const SymbolRef m_symbol;

	template<typename T>
	using BuiltinFunction = BaseExpressionRef (T::*) (
		const BaseExpressionRef *,
		size_t,
		const Evaluation &);

    template<typename T>
    using ExtendedBuiltinFunction = BaseExpressionRef (T::*) (
        ExpressionPtr,
        const Evaluation &);

    template<typename T>
    using ExtendedF0 = BaseExpressionRef (T::*) (
        const EmptyExpression &,
        const Evaluation &);

    template<typename T>
    using F1 = BaseExpressionRef (T::*) (
		BaseExpressionPtr,
        const Evaluation &);

	template<typename T>
	using ExtendedF1 = BaseExpressionRef (T::*) (
		ExpressionPtr,
		BaseExpressionPtr,
		const Evaluation &);

	template<typename T>
	using B1 = bool (T::*) (
		BaseExpressionPtr,
		const Evaluation &);

	template<typename T, typename Options>
	using OptionsPatternF1 = BaseExpressionRef (T::*) (
		BaseExpressionPtr,
		const Options&,
		const Evaluation &);

	template<typename T, typename Options>
	using ExtendedOptionsPatternF1 = BaseExpressionRef (T::*) (
		ExpressionPtr,
		BaseExpressionPtr,
		const Options&,
		const Evaluation &);

	template<typename T>
    using F2 = BaseExpressionRef (T::*) (
		BaseExpressionPtr,
		BaseExpressionPtr,
        const Evaluation &);

	template<typename T>
	using ExtendedF2 = BaseExpressionRef (T::*) (
		ExpressionPtr,
		BaseExpressionPtr,
		BaseExpressionPtr,
		const Evaluation &);

	template<typename T, typename Options>
	using OptionsPatternF2 = BaseExpressionRef (T::*) (
		BaseExpressionPtr,
		BaseExpressionPtr,
		const Options&,
		const Evaluation &);

	template<typename T, typename Options>
	using ExtendedOptionsPatternF2 = BaseExpressionRef (T::*) (
		ExpressionPtr,
		BaseExpressionPtr,
		BaseExpressionPtr,
		const Options&,
		const Evaluation &);

    template<typename T>
    using F3 = BaseExpressionRef (T::*) (
		BaseExpressionPtr,
		BaseExpressionPtr,
		BaseExpressionPtr,
        const Evaluation &);

	template<typename T, typename Options>
	using OptionsPatternF3 = BaseExpressionRef (T::*) (
		BaseExpressionPtr,
		BaseExpressionPtr,
		BaseExpressionPtr,
		const Options&,
		const Evaluation &);

	template<typename T, typename Options>
	using ExtendedOptionsPatternF3 = BaseExpressionRef (T::*) (
		ExpressionPtr,
		BaseExpressionPtr,
		BaseExpressionPtr,
		BaseExpressionPtr,
		const Options&,
		const Evaluation &);

    template<typename T>
    using F4 = BaseExpressionRef (T::*) (
        BaseExpressionPtr,
        BaseExpressionPtr,
        BaseExpressionPtr,
        BaseExpressionPtr,
        const Evaluation &);

    template<typename T>
    using F5 = BaseExpressionRef (T::*) (
        BaseExpressionPtr,
        BaseExpressionPtr,
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

	template<typename T, typename Options>
	using ExtendedOptionsF2 = BaseExpressionRef (T::*) (
		ExpressionPtr,
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

    template<typename T, typename Options>
    using ExtendedOptionsF3 = BaseExpressionRef (T::*) (
		ExpressionPtr,
        BaseExpressionPtr,
        BaseExpressionPtr,
        BaseExpressionPtr,
        const Options &options,
        const Evaluation &);

private:
    template<typename T, typename Add>
    inline void add_rule(BuiltinFunction<T> method, const Add &add) {
        const auto self = shared_from_this<T>();

        const auto function = [self, method] (
            ExpressionPtr,
            const BaseExpressionRef *leaves,
            size_t n,
            const Evaluation &evaluation) {

			auto p = self.get();
			return (p->*method)(leaves, n, evaluation);
        };

        add(VariadicBuiltinRule<0, decltype(function)>::construct(
            m_symbol,
            m_runtime.evaluation(),
            function));
    }

    template<typename T, typename Add>
    inline void add_rule(ExtendedBuiltinFunction<T> method, const Add &add) {
        const auto self = shared_from_this<T>();

        const auto function = [self, method] (
            ExpressionPtr expr,
            const BaseExpressionRef *,
            size_t,
            const Evaluation &evaluation) {

            auto p = self.get();
            return (p->*method)(expr, evaluation);
        };

        add(VariadicBuiltinRule<0, decltype(function)>::construct(
            m_symbol,
            m_runtime.evaluation(),
            function));
    }

    template<typename T, typename Options, typename Add, typename F, int N, template<typename, typename, int> class B>
    inline void internal_add_options_rule(const OptionsInitializerList &options, F function, const Add &add) {
        const auto bridge = B<T, F, N>(this, function);

        add(OptionsBuiltinRule<N, Options, decltype(bridge)>::construct(
            m_symbol,
            m_runtime.evaluation(),
            options,
            bridge));
    }

    template<typename T, typename Options, typename Add>
    inline void add_rule(const OptionsInitializerList &options, OptionsF2<T, Options> function, const Add &add) {
        return internal_add_options_rule<T, Options, Add, decltype(function), 2, ArgumentsBridge>(
            options, function, add);
    }

    template<typename T, typename Options, typename Add>
    inline void add_rule(const OptionsInitializerList &options, ExtendedOptionsF2<T, Options> function, const Add &add) {
        return internal_add_options_rule<T, Options, Add, decltype(function), 2, ExtendedArgumentsBridge>(
            options, function, add);
    }

    template<typename T, typename Options, typename Add>
    inline void add_rule(const OptionsInitializerList &options, OptionsF3<T, Options> function, const Add &add) {
        return internal_add_options_rule<T, Options, Add, decltype(function), 3, ArgumentsBridge>(
            options, function, add);
    }

    template<typename T, typename Options, typename Add>
    inline void add_rule(const OptionsInitializerList &options, ExtendedOptionsF3<T, Options> function, const Add &add) {
        return internal_add_options_rule<T, Options, Add, decltype(function), 3, ExtendedArgumentsBridge>(
            options, function, add);
    }

    template<typename T, typename Add, typename F, int N, template<typename, typename, int> class B>
    inline void internal_add_arguments_rule(F function, const Add &add) {
        const auto bridge = B<T, F, N>(this, function);

        add(BuiltinRule<N, decltype(bridge)>::construct(
            m_symbol,
            m_runtime.evaluation(),
            bridge));
    }

    template<typename T, typename Add>
    inline void add_rule(ExtendedF0<T> function, const Add &add) {
        return internal_add_arguments_rule<T, Add, decltype(function), 0, ExtendedArgumentsBridge>(function, add);
    }

    template<typename T, typename Add>
    inline void add_rule(F1<T> function, const Add &add) {
        return internal_add_arguments_rule<T, Add, decltype(function), 1, ArgumentsBridge>(function, add);
    }

    template<typename T, typename Add>
    inline void add_rule(ExtendedF1<T> function, const Add &add) {
        return internal_add_arguments_rule<T, Add, decltype(function), 1, ExtendedArgumentsBridge>(function, add);
    }

    template<typename T, typename Add>
    inline void add_rule(B1<T> function, const Add &add) {
        return internal_add_arguments_rule<T, Add, decltype(function), 1, BooleanBridge>(function, add);
    }

    template<typename T, typename Add>
    inline void add_rule(F2<T> function, const Add &add) {
        return internal_add_arguments_rule<T, Add, decltype(function), 2, ArgumentsBridge>(function, add);
    }

    template<typename T, typename Add>
    inline void add_rule(ExtendedF2<T> function, const Add &add) {
        return internal_add_arguments_rule<T, Add, decltype(function), 2, ExtendedArgumentsBridge>(function, add);
    }

    template<typename T, typename Add>
    inline void add_rule(F3<T> function, const Add &add) {
        return internal_add_arguments_rule<T, Add, decltype(function), 3, ArgumentsBridge>(function, add);
    }

    /*template<typename T, typename Add>
    inline void add_rule(ExtendedF3<T> function, const Add &add) {
        return internal_add_arguments_rule<T, Add, decltype(function), 3, ExtendedArgumentsBridge>(function, add);
    }*/

    template<typename T, typename Add>
    inline void add_rule(F4<T> function, const Add &add) {
        return internal_add_arguments_rule<T, Add, decltype(function), 4, ArgumentsBridge>(function, add);
    }

    /*template<typename T, typename Add>
    inline void add_rule(ExtendedF4<T> function, const Add &add) {
        return internal_add_arguments_rule<T, Add, decltype(function), 4, ExtendedArgumentsBridge>(function, add);
    }*/

    template<typename T, typename Add>
    inline void add_rule(F5<T> function, const Add &add) {
        return internal_add_arguments_rule<T, Add, decltype(function), 5, ArgumentsBridge>(function, add);
    }

    template<typename T, typename Add, typename F, int N, template<typename, typename, int> class B>
    inline void internal_add_pattern_rule(const char *pattern, F function, const Add &add) {
        const auto bridge = B<T, F, N>(this, function);
        const BaseExpressionRef p = m_runtime.parse(pattern);
	    add(PatternMatchedBuiltinRule<N, decltype(bridge)>::construct(p, bridge, m_runtime.evaluation()));
    }

	template<typename T, typename Options, typename Add, typename F, int N, template<typename, typename, int> class B>
	inline void internal_add_options_pattern_rule(const char *pattern, F function, const Add &add) {
		const auto bridge = B<T, F, N>(this, function);
		const BaseExpressionRef p = m_runtime.parse(pattern);
		add(PatternMatchedOptionsBuiltinRule<N, Options, decltype(bridge)>::construct(
			p, bridge, OptionsDefinitions<Options>(m_runtime.definitions()), m_runtime.evaluation()));
	}

    template<typename T, typename Add>
    inline void add_rule(const char *pattern, F1<T> function, const Add &add) {
        internal_add_pattern_rule<T, Add, decltype(function), 1, ArgumentsBridge>(pattern, function, add);
    }

    template<typename T, typename Add>
    inline void add_rule(const char *pattern, F2<T> function, const Add &add) {
        internal_add_pattern_rule<T, Add, decltype(function), 2, ArgumentsBridge>(pattern, function, add);
    }

    template<typename T, typename Add>
    inline void add_rule(const char *pattern, F3<T> function, const Add &add) {
        internal_add_pattern_rule<T, Add, decltype(function), 3, ArgumentsBridge>(pattern, function, add);
    }

    template<typename T, typename Add>
    inline void add_rule(const char *pattern, F4<T> function, const Add &add) {
        internal_add_pattern_rule<T, Add, decltype(function), 4, ArgumentsBridge>(pattern, function, add);
    }

    template<typename T, typename Add>
    inline void add_rule(const char *pattern, F5<T> function, const Add &add) {
        internal_add_pattern_rule<T, Add, decltype(function), 5, ArgumentsBridge>(pattern, function, add);
    }

	template<typename T, typename Options, typename Add>
	inline void add_rule(const char *pattern, OptionsPatternF1<T, Options> function, const Add &add) {
		internal_add_options_pattern_rule<T, Options, Add, decltype(function), 1, ArgumentsBridge>(pattern, function, add);
	}

	template<typename T, typename Options, typename Add>
	inline void add_rule(const char *pattern, ExtendedOptionsPatternF1<T, Options> function, const Add &add) {
		internal_add_options_pattern_rule<T, Options, Add, decltype(function), 1, ExtendedArgumentsBridge>(pattern, function, add);
	}

	template<typename T, typename Options, typename Add>
	inline void add_rule(const char *pattern, OptionsPatternF2<T, Options> function, const Add &add) {
		internal_add_options_pattern_rule<T, Options, Add, decltype(function), 2, ArgumentsBridge>(pattern, function, add);
	}

	template<typename T, typename Options, typename Add>
	inline void add_rule(const char *pattern, ExtendedOptionsPatternF2<T, Options> function, const Add &add) {
		internal_add_options_pattern_rule<T, Options, Add, decltype(function), 2, ExtendedArgumentsBridge>(pattern, function, add);
	}

	template<typename T, typename Options, typename Add>
	inline void add_rule(const char *pattern, OptionsPatternF3<T, Options> function, const Add &add) {
		internal_add_options_pattern_rule<T, Options, Add, decltype(function), 3, ArgumentsBridge>(pattern, function, add);
	}

	template<typename T, typename Options, typename Add>
	inline void add_rule(const char *pattern, ExtendedOptionsPatternF3<T, Options> function, const Add &add) {
		internal_add_options_pattern_rule<T, Options, Add, decltype(function), 3, ExtendedArgumentsBridge>(pattern, function, add);
	}

    /*template<typename T, typename Add>
	inline void add_rule(const char *pattern, const char *into, const Add &add) {
		const BaseExpressionRef lhs = m_runtime.parse(pattern);
		const BaseExpressionRef rhs = m_runtime.parse(into);
		add(lhs.get(), rhs.get());
	}*/

protected:
    template<typename F>
    inline void builtin(F function) {
        add_rule(function, [this] (const RuleRef &rule) {
            m_symbol->mutable_state().add_rule(rule, m_runtime.evaluation());
        });
    }

	template<typename F>
	inline void builtin(const OptionsInitializerList &options, F function) {
        add_rule(options, function, [this] (const RuleRef &rule) {
            m_symbol->mutable_state().add_rule(rule, m_runtime.evaluation());
        });
	}

    template<typename F>
    inline void builtin(const char *pattern, F function) {
        add_rule(pattern, function, [this] (const RuleRef &rule) {
	        rule_owner(rule->pattern).add_rule(rule, m_runtime.evaluation());
        });
    }

	inline void builtin(const char *pattern, const char *into) {
		const BaseExpressionRef lhs = m_runtime.parse(pattern);
		const BaseExpressionRef rhs = m_runtime.parse(into);
		rule_owner(lhs).add_rule(lhs.get(), rhs.get(), m_runtime.evaluation());
	}

	inline void builtin(const BaseExpressionRef &lhs, const char *into) {
		const BaseExpressionRef rhs = m_runtime.parse(into);
		rule_owner(lhs).add_rule(lhs.get(), rhs.get(), m_runtime.evaluation());
	}

	inline void builtin(const BaseExpressionRef &lhs, const BaseExpressionRef &rhs) {
		rule_owner(lhs).add_rule(lhs.get(), rhs.get(), m_runtime.evaluation());
	}

    template<typename T>
    inline void builtin() {
	    const auto &evaluation = m_runtime.evaluation();
        m_symbol->mutable_state().add_rule(
		    RuleRef(T::construct(m_symbol, evaluation)),
		    evaluation);
    }

	template<typename F>
	inline void format(F function, const SymbolRef &form) {
		add_rule(function, [this, &form] (const RuleRef &rule) {
			m_symbol->mutable_state().add_format(rule, form, m_runtime.evaluation());
		});
	}

    inline void format(const BaseExpressionRef &lhs, const char *into, const std::initializer_list<SymbolPtr> &forms) {
		const BaseExpressionRef rhs = m_runtime.parse(into);
		assert(lhs->is_expression() && lhs->as_expression()->head()->deverbatim() == m_symbol.get());
        auto &evaluation = m_runtime.evaluation();
	    for (SymbolPtr form : forms) {
		    m_symbol->mutable_state().add_format(DownRule::construct(lhs, rhs, evaluation), form, evaluation);
	    }
	}

	inline void format(const BaseExpressionRef &lhs, const char *into) {
		return format(lhs, into, {m_runtime.symbols().All});
	}

	inline void format(const char *pattern, const char *into, const std::initializer_list<SymbolPtr> &forms) {
		const BaseExpressionRef lhs = m_runtime.parse(pattern);
		format(lhs, into);
	}

	inline void format(const char *pattern, const char *into) {
		return format(pattern, into, {m_runtime.symbols().All});
	}

	inline bool has_format(const char *pattern) const {
		return m_symbol->state().has_format(
			m_runtime.parse(pattern), m_runtime.evaluation());
	}

	inline void message(const char *tag, const char *text) {
		m_symbol->add_message(tag, text, m_runtime.evaluation());
	}

public:
    static constexpr auto attributes = Attributes::None;

#if MAKE_UNIT_TEST
	static const char *docs;
#endif

    Builtin(Runtime &runtime, const SymbolRef &symbol, Definitions &definitions) :
		m_runtime(runtime), m_symbol(symbol) {
	}

    virtual ~Builtin() {
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
		const std::initializer_list<NewRuleRef> &rules,
		const char *docs = "") const {

		m_runtime.add(name, attributes, rules);
#if MAKE_UNIT_TEST
		m_runtime.add_docs(name, docs);
#endif
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
	Unit(Runtime &runtime) :
		m_runtime(runtime) {
	}
};

#endif //CMATHICS_RUNTIME_H
