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
class BuiltinRule : public Rule {
private:
	const F _func;

public:
	BuiltinRule(const F &func) : _func(func) {
	}

	virtual BaseExpressionRef try_apply(const ExpressionRef &expr, const Evaluation &evaluation) const {
		if (N <= MaxStaticSliceSize || expr->size() == N) {
			constexpr SliceCode slice_code = N <= MaxStaticSliceSize ? static_slice_code(N) : SliceCode::Unknown;
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

	virtual DefinitionsPos get_definitions_pos(const SymbolRef &symbol) const {
		return DefinitionsPos::Down;
	}

	virtual MatchSize match_size() const {
		return MatchSize::exactly(N);
	}
};

template<int N, typename F>
inline RuleRef make_builtin_rule(const F &func) {
	return std::make_shared<BuiltinRule<N, F>>(func);
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

	virtual BaseExpressionRef try_apply(const ExpressionRef &expr, const Evaluation &evaluation) const {
		const Match m = match(_patt, expr, evaluation.definitions);
		if (m) {
			return apply_from_tuple(_func, std::tuple_cat(m.get<N>(), std::make_tuple(evaluation)));
		} else {
			return BaseExpressionRef();
		}
	}

	virtual DefinitionsPos get_definitions_pos(const SymbolRef &symbol) const {
		if (_patt->head() == symbol) {
			return DefinitionsPos::Down;
		} else if ( _patt->lookup_name() == symbol) {
			return DefinitionsPos::Sub;
		} else {
			return DefinitionsPos::None;
		}
	}

	virtual MatchSize match_size() const {
		return _patt->match_size();
	}
};

template<int N>
inline RuleRef make_pattern_matched_builtin_rule(
	const BaseExpressionRef &patt, typename BuiltinFunctionArguments<N>::type func) {

	return std::make_shared<PatternMatchedBuiltinRule<N>>(patt, func);
}

class RewriteRule : public Rule {
private:
	const BaseExpressionRef _patt;
	const BaseExpressionRef _into;

public:
	RewriteRule(const BaseExpressionRef &patt, const BaseExpressionRef &into) :
		_patt(patt), _into(into) {
	}

	virtual BaseExpressionRef try_apply(const ExpressionRef &expr, const Evaluation &evaluation) const {
		const Match m = match(_patt, expr, evaluation.definitions);
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

	virtual DefinitionsPos get_definitions_pos(const SymbolRef &symbol) const {
		if (_patt->head() == symbol) {
			return DefinitionsPos::Down;
		} else if ( _patt->lookup_name() == symbol) {
			return DefinitionsPos::Sub;
		} else {
			return DefinitionsPos::None;
		}
	}

	virtual MatchSize match_size() const {
		return _patt->match_size();
	}
};

inline RuleRef make_rewrite_rule(const BaseExpressionRef &patt, const BaseExpressionRef &into) {
	return std::make_shared<RewriteRule>(patt, into);
}

// as Function is a very important pattern, we provide a special optimized Rule for it.

class FunctionRule : public Rule {
public:
	virtual BaseExpressionRef try_apply(const ExpressionRef &expr, const Evaluation &evaluation) const {
		// match and unpack Function[body_][args___]

		const BaseExpressionRef &head = expr->_head;
		if (head->type() != ExpressionType) {
			return BaseExpressionRef();
		}
		const Expression *expr_head = static_cast<const Expression*>(head.get());
		if (expr_head->slice_code() != SliceCode::StaticSlice0Code + 1) {
			return BaseExpressionRef();
		}
		const BaseExpressionRef &body = static_cast<const StaticSlice<1>*>(expr_head->_slice_ptr)->leaf(0);
		if (body->type() != ExpressionType) {
			return BaseExpressionRef();
		}

		const Expression *expr_body = static_cast<const Expression*>(body.get());
		return expr->with_leaves_array([expr_body, &evaluation] (const BaseExpressionRef *slots, size_t n_slots) {
			return expr_body->replace_slots(slots, n_slots, evaluation);
		});
	}

	virtual DefinitionsPos get_definitions_pos(const SymbolRef &symbol) const {
		return DefinitionsPos::Sub; // assuming "symbol" is Function
	}

	virtual MatchSize match_size() const {
		return MatchSize::at_least(0);
	}};
