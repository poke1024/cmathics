#ifndef CMATHICS_BUILTIN_H
#define CMATHICS_BUILTIN_H

#include "evaluation.h"
#include "core/pattern/arguments.h"
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
	return RuleRef(T::construct(head, definitions));
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
class BuiltinRule : public ExactlyNRule<N>, public ExtendedHeapObject<BuiltinRule<N, F>> {
private:
	const F m_func;

public:
	BuiltinRule(const SymbolRef &head, const Evaluation &evaluation, const F &func) :
		ExactlyNRule<N>(head, evaluation), m_func(func) {
	}

	virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

		constexpr SliceCode slice_code =
			N <= MaxTinySliceSize ? tiny_slice_code(N) : SliceCode::Unknown;

        const BaseExpressionRef result = expr->with_leaves_array<slice_code>(
			[this, expr, &evaluation] (const BaseExpressionRef *leaves, size_t size) {
				typename BaseExpressionTuple<N>::type t;
				unpack_leaves<N, 0>()(leaves, t);

				return apply_from_tuple(
                    m_func,
					std::tuple_cat(std::forward_as_tuple(expr), t, std::forward_as_tuple(evaluation)));
			});

        return result;
	}
};

template<int N, typename F>
class VariadicBuiltinRule :
    public AtLeastNRule<N>,
    public ExtendedHeapObject<VariadicBuiltinRule<N, F>> {

private:
	const F _func;

public:
	VariadicBuiltinRule(const SymbolRef &head, const Evaluation &evaluation, const F &func) :
		AtLeastNRule<N>(head, evaluation), _func(func) {
	}

	virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

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
	std::unordered_map<SymbolRef, size_t, SymbolHash, SymbolEqual> m_offsets;
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

	OptionsDefinitions(Definitions &definitions) {
		initialize(definitions, Options::meta(), reinterpret_cast<uint8_t*>(&m_defaults), sizeof(m_defaults));
	}

	inline const Options &defaults() const {
		return m_defaults;
	}

	inline bool set(
        Options &options,
        const SymbolPtr key,
        const BaseExpressionRef &value,
        const Evaluation &evaluation) const {

		const auto i = m_offsets.find(key);

		if (i != m_offsets.end()) {
			set_ptr(reinterpret_cast<uint8_t*>(&options), i->second, value);
			return true;
		}

		return false;
	}
};

template<int N, typename Options, typename F>
class OptionsBuiltinRule : public AtLeastNRule<N>, public ExtendedHeapObject<OptionsBuiltinRule<N, Options, F>> {
private:
	const SymbolRef m_head;
	const F m_func;
	const OptionsDefinitions<Options> m_options;

	struct OptionMissing {
		SymbolRef name;
	};

public:
	OptionsBuiltinRule(
		const SymbolRef &head,
		const Evaluation &evaluation,
		const OptionsInitializerList &options,
		const F &func) :

		AtLeastNRule<N>(head, evaluation), m_head(head), m_func(func), m_options(evaluation.definitions, options) {
	}

	virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

		return expr->with_leaves_array(
			[this, expr, &evaluation]
			(const BaseExpressionRef *leaves, size_t size) -> optional<BaseExpressionRef> {
				Options options;
				const Options *options_ptr;

				// parse options. this should behave exactly as if it were an OptionsPattern[].
				const BaseExpressionRef *i = leaves + N;
				const BaseExpressionRef *const end = leaves + size;
				if (i == end) {
					options_ptr = &m_options.defaults();
				} else {
					try {
						options = m_options.defaults();
						options_ptr = &options;

						const auto &assign = [this, &options, &evaluation] (
							SymbolPtr name, const BaseExpressionRef &value) {

							if (!m_options.set(options, name, value, evaluation)) {
								throw OptionMissing{name};
							}
						};

						while (i < end) {
							if (!parse_options(assign, *i, evaluation)) {
								// rule does not apply, as remaining arguments are not options
								// and thus do not match OptionsPattern[].
								return optional<BaseExpressionRef>();
							}
							i++;
						}
					} catch (const OptionMissing &missing) {
						evaluation.message(m_head, "optx", missing.name, BaseExpressionRef(expr));
						return BaseExpressionRef();
					}
				}

				typename BaseExpressionTuple<N>::type t;
				unpack_leaves<N, 0>()(leaves, t);
				return apply_from_tuple(
					m_func,
					std::tuple_cat(std::forward_as_tuple(expr),
					t,
					std::forward_as_tuple(*options_ptr, evaluation)));
			});
	}
};

typedef std::function<RuleRef(const SymbolRef &head, Evaluation &evaluation)> NewRuleRef;

template<int N, typename F>
inline NewRuleRef make_builtin_rule(const F &func) {
	return [func] (const SymbolRef &head, Evaluation &evaluation) -> RuleRef {
		return BuiltinRule<N, F>::construct(head, evaluation, func);
	};
}

template<typename Matcher>
class RewriteRule : public Rule, public ExtendedHeapObject<RewriteRule<Matcher>> {
private:
	const BaseExpressionRef m_into;
	const Matcher m_matcher;
	const RewriteBaseExpression m_rewrite;

public:
	RewriteRule(const BaseExpressionRef &patt, const BaseExpressionRef &into, const Evaluation &evaluation) :
		Rule(patt, evaluation), m_into(into), m_matcher(patt),
		m_rewrite(m_matcher.prepare(into, evaluation)) {
	}

	virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

		const MatchRef match = m_matcher(expr, evaluation);
		if (match) {
            const auto slot = [&match] (index_t i, const BaseExpressionRef &prev) {
                const BaseExpressionRef &slot = match->slot(i);
                if (slot) { // was this variable assigned?
                    return slot;
                } else {
                    return prev;
                }
            };

            return m_rewrite.rewrite_root_or_copy(
                m_into->as_expression(),
                slot,
                match->options(),
                evaluation);
		} else {
			return optional<BaseExpressionRef>();
		}
	}

	virtual BaseExpressionRef rhs() const {
		return m_into;
	}
};

#endif // CMATHICS_BUILTIN_H
