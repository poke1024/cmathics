#ifndef CMATHICS_BUILTIN_H
#define CMATHICS_BUILTIN_H

#include "evaluation.h"
#include "pattern.h"
#include "definitions.h"

// apply_from_tuple is taken from http://www.cppsamples.com/common-tasks/apply-tuple-to-function.html
// in C++17, this will become std::apply

template<typename F, typename Tuple, size_t ...S >
decltype(auto) apply_tuple_impl(F&& fn, Tuple&& t, std::index_sequence<S...>)
{
    return std::forward<F>(fn)(std::get<S>(std::forward<Tuple>(t))...);
}

template<typename F, typename Tuple>
decltype(auto) apply_from_tuple(F&& fn, Tuple&& t)
{
    std::size_t constexpr tSize
            = std::tuple_size<typename std::remove_reference<Tuple>::type>::value;
    return apply_tuple_impl(std::forward<F>(fn),
                            std::forward<Tuple>(t),
                            std::make_index_sequence<tSize>());
}

template<typename T>
RuleRef NewRule(const SymbolRef &head, const Definitions &definitions) {
	return RuleRef(new T(head, definitions));
}

template<int N, typename... refs>
struct BuiltinFunctionArguments {
    typedef typename BuiltinFunctionArguments<N - 1, BaseExpressionPtr, refs...>::type type;
};

template<typename... refs>
struct BuiltinFunctionArguments<0, refs...> {
    typedef std::function<BaseExpressionRef(refs..., const Evaluation&)> type;
};

template<int M, int N>
struct unpack_leaves {
	void operator()(const BaseExpressionRef *leaves, typename BaseExpressionTuple<M>::type &t) {
		// symbols are already ordered in the order of their (first) appearance in the original pattern.
		std::get<N>(t) = leaves->get();
		unpack_leaves<M, N + 1>()(leaves + 1, t);
	}
};

template<int M>
struct unpack_leaves<M, M> {
	void operator()(const BaseExpressionRef *leaves, typename BaseExpressionTuple<M>::type &t) {
	}
};

template<int N, typename F>
class BuiltinRule : public ExactlyNRule<N> {
private:
	const F _func;

public:
	BuiltinRule(const SymbolRef &head, const Definitions &definitions, const F &func) :
		ExactlyNRule<N>(head, definitions), _func(func) {
	}

	virtual BaseExpressionRef try_apply(const Expression *expr, const Evaluation &evaluation) const {
		constexpr SliceCode slice_code =
			N <= MaxStaticSliceSize ? static_slice_code(N) : SliceCode::Unknown;
		const F &func = _func;
		return expr->with_leaves_array<slice_code>(
			[expr, &func, &evaluation] (const BaseExpressionRef *leaves, size_t size) {
				typename BaseExpressionTuple<N>::type t;
				unpack_leaves<N, 0>()(leaves, t);
				return apply_from_tuple(
					func,
					std::tuple_cat(std::forward_as_tuple(expr), t, std::forward_as_tuple(evaluation)));
			});
	}
};

template<int N, typename F>
class VariadicBuiltinRule : public AtLeastNRule<N> {
private:
	const F _func;

public:
	VariadicBuiltinRule(const SymbolRef &head, const Definitions &definitions, const F &func) :
		AtLeastNRule<N>(head, definitions), _func(func) {
	}

	virtual BaseExpressionRef try_apply(const Expression *expr, const Evaluation &evaluation) const {
		const F &func = _func;
		return expr->with_leaves_array(
			[expr, &func, &evaluation](const BaseExpressionRef *leaves, size_t size) {
				return func(expr, leaves, size, evaluation);
			});
	}
};

inline std::string ensure_context(const char *name) {
	return std::string("System`") + name;
}

typedef const std::initializer_list<std::tuple<const char*, size_t, const char*>> OptionsInitializerList;

class OptionsDefinitionsBase {
protected:
	std::map<BaseExpressionRef, size_t> m_offsets;
	std::vector<BaseExpressionRef> m_values;

	inline void set_ptr(uint8_t *data, size_t offset, const BaseExpressionRef &value) const {
		*reinterpret_cast<BaseExpressionPtr*>(data + offset) = value.get();
	}

	void initialize(
		Definitions &definitions,
		const OptionsInitializerList &options,
		uint8_t *data,
		size_t size) {

		std::memset(data, 0, size);
		for (auto t : options) {
			const char *name = std::get<0>(t);
			const size_t offset = std::get<1>(t);
			const BaseExpressionRef value = definitions.lookup(ensure_context(std::get<2>(t)).c_str());
			const SymbolRef symbol = definitions.lookup(ensure_context(name).c_str());

			m_offsets[symbol] = offset;
			m_values.push_back(value);
			assert(offset + sizeof(BaseExpressionPtr) <= size);
			set_ptr(data, offset, value);
		}
	}
};

template<typename Options>
class OptionsDefinitions : public OptionsDefinitionsBase {
private:
	Options m_defaults;

public:
	OptionsDefinitions(Definitions &definitions, const OptionsInitializerList &options) {
		initialize(definitions, options, reinterpret_cast<uint8_t*>(&m_defaults), sizeof(m_defaults));
	}

	inline void initialize_defaults(Options &options) const {
		std::memcpy(&options, &m_defaults, sizeof(Options));
	}

	inline bool set(Options &options, const BaseExpressionRef &key, const BaseExpressionRef &value) const {
		const auto i = m_offsets.find(key);

		if (i != m_offsets.end()) {
			set_ptr(reinterpret_cast<uint8_t*>(&options), i->second, value);
			return true;
		}

		return false;
	}
};

template<int N, typename Options, typename F>
class OptionsBuiltinRule : public AtLeastNRule<N> {
private:
	const SymbolRef m_head;
	const F m_func;
	const OptionsDefinitions<Options> m_options;

public:
	OptionsBuiltinRule(
		const SymbolRef &head,
		Definitions &definitions,
		const OptionsInitializerList &options,
		const F &func) :

		AtLeastNRule<N>(head, definitions), m_head(head), m_func(func), m_options(definitions, options) {
	}

	virtual BaseExpressionRef try_apply(const Expression *expr, const Evaluation &evaluation) const {
		const SymbolRef &head = m_head;
		const F &func = m_func;
		const OptionsDefinitions<Options> &options_definitions = m_options;

		return expr->with_leaves_array(
			[expr, &head, &func, &options_definitions, &evaluation]
			(const BaseExpressionRef *leaves, size_t size) {

				Options options;
				options_definitions.initialize_defaults(options);

				size_t n = size;
				while (n > N) {
					const BaseExpressionRef &last = leaves[n - 1];

					if (last->has_form(S::Rule, 2, evaluation)) {
						const BaseExpressionRef * const option =
							last->as_expression()->n_leaves<2>();

						const BaseExpressionRef &key = option[0];
						const BaseExpressionRef &value = option[1];

						bool ok;

						if (key->is_string()) {
							const BaseExpressionRef resolved_key =
								static_cast<const String*>(key.get())->option_symbol(evaluation);
							ok = options_definitions.set(options, resolved_key, value);
						} else {
							ok = options_definitions.set(options, key, value);
						}

						if (!ok) {
							evaluation.message(head, "optx", key, BaseExpressionRef(expr));
							return BaseExpressionRef();
						}
					} else {
						// FIXME: option expected
					}

					n -= 1;
				}

				typename BaseExpressionTuple<N>::type t;
				unpack_leaves<N, 0>()(leaves, t);
				return apply_from_tuple(
					func,
					std::tuple_cat(std::forward_as_tuple(expr), t, std::forward_as_tuple(options, evaluation)));
			});
	}
};

typedef std::function<RuleRef(const SymbolRef &head, Definitions &definitions)> NewRuleRef;

template<int N, typename F>
inline NewRuleRef make_builtin_rule(const F &func) {
	return [func] (const SymbolRef &head, Definitions &definitions) -> RuleRef {
		return new BuiltinRule<N, F>(head, definitions, func);
	};
}

template<typename Matcher>
class RewriteRule : public Rule {
private:
	const BaseExpressionRef m_into;
	const Matcher m_matcher;
	const RewriteBaseExpression m_rewrite;

public:
	RewriteRule(const BaseExpressionRef &patt, const BaseExpressionRef &into) :
		Rule(patt), m_into(into), m_matcher(patt), m_rewrite(m_matcher.prepare(into)) {
	}

	virtual BaseExpressionRef try_apply(const Expression *expr, const Evaluation &evaluation) const {
		const MatchRef m = m_matcher(expr, evaluation);
		if (m) {
            return m_rewrite.rewrite_or_copy(
                m_into->as_expression(),
                [&m] (index_t i, const BaseExpressionRef &prev) {
                    const BaseExpressionRef &slot = m->slot(i);
                    if (slot) { // was this variable assigned?
                        return slot;
                    } else {
                        return prev;
                    }
                });
		} else {
			return BaseExpressionRef();
		}
	}

	virtual BaseExpressionRef rhs() const {
		return m_into;
	}
};

#endif // CMATHICS_BUILTIN_H
