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

template<int N, typename... refs>
struct BuiltinFunctionArguments {
    typedef typename BuiltinFunctionArguments<N - 1, BaseExpressionRef, refs...>::type type;
};

template<typename... refs>
struct BuiltinFunctionArguments<0, refs...> {
    typedef std::function<BaseExpressionRef(refs..., const Evaluation&)> type;
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
		if (N <= MaxStaticSliceSize || expr->size() == N) {
			constexpr SliceCode slice_code =
				N <= MaxStaticSliceSize ? static_slice_code(N) : SliceCode::Unknown;
			const F &func = _func;
			return expr->with_leaves_array<slice_code>(
				[&func, &evaluation] (const BaseExpressionRef *leaves, size_t size) {
					typename BaseExpressionTuple<N>::type t;
					unpack_leaves<N, 0>()(leaves, t);
					return apply_from_tuple(
						func,
						std::tuple_cat(t, std::make_tuple(evaluation)));
				});
		} else {
			return BaseExpressionRef();
		}
	}
};

typedef std::function<RuleRef(const SymbolRef &head, const Definitions &definitions)> NewRuleRef;

template<int N, typename F>
inline NewRuleRef make_builtin_rule(const F &func) {
	return [func] (const SymbolRef &head, const Definitions &definitions) -> RuleRef {
		return std::make_shared<BuiltinRule<N, F>>(head, definitions, func);
	};
}

// note: PatternMatchedBuiltinRule should only be used for builtins that match non-down values (e.g. sub values).
// if you write builtins that match down values, always use BuiltinRule, since it's faster (it doesn't involve the
// pattern matcher).

template<int N>
class PatternMatchedBuiltinRule : public Rule {
private:
	const BaseExpressionRef _patt;
	typename BuiltinFunctionArguments<N>::type _func;

public:
	PatternMatchedBuiltinRule(const BaseExpressionRef &patt, typename BuiltinFunctionArguments<N>::type func) :
		_patt(patt), _func(func) {
	}

	virtual BaseExpressionRef try_apply(const Expression *expr, const Evaluation &evaluation) const {
		const Match m = match(_patt, expr, evaluation.definitions);
		if (m) {
			return apply_from_tuple(_func, std::tuple_cat(m.get<N>(), std::make_tuple(evaluation)));
		} else {
			return BaseExpressionRef();
		}
	}

	virtual MatchSize match_size() const {
		return _patt->match_size();
	}

	virtual SortKey pattern_key() const {
		return _patt->pattern_key();
	}
};

template<int N>
inline NewRuleRef make_pattern_matched_builtin_rule(
	const BaseExpressionRef &patt, typename BuiltinFunctionArguments<N>::type func) {

	return [patt, func] (const SymbolRef &head, const Definitions &definitions) -> RuleRef {
		return std::make_shared<PatternMatchedBuiltinRule<N>>(patt, func);
	};
}

class RewriteRule : public Rule {
private:
	const BaseExpressionRef _into;

public:
	RewriteRule(const BaseExpressionRef &patt, const BaseExpressionRef &into) :
		Rule(patt), _into(into) {
	}

	virtual BaseExpressionRef try_apply(const Expression *expr, const Evaluation &evaluation) const {
		const Match m = match(pattern, expr, evaluation.definitions);
		if (m) {
			const BaseExpressionRef replaced = _into->replace_all(m);
			if (!replaced) {
				return _into;
			} else {
				return replaced;
			}
		} else {
			return BaseExpressionRef();
		}
	}

	virtual BaseExpressionRef rhs() const {
		return _into;
	}
};

inline NewRuleRef make_rewrite_rule(const BaseExpressionRef &patt, const BaseExpressionRef &into) {
	return [patt, into] (const SymbolRef &head, const Definitions &definitions) -> RuleRef {
		return std::make_shared<RewriteRule>(patt, into);
	};
}

#endif // CMATHICS_BUILTIN_H
