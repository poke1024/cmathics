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

// note: PatternMatchedBuiltinRule is only provided here for compatibility reasons. for new builtin functions,
// you should always use BuiltinRule, as it's faster than PatternMatchedBuiltinRule.

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
		return MatchSize(0); // FIXME; inspect _patt
	}
};

template<int N>
inline RuleRef make_builtin_rule(const BaseExpressionRef &patt, typename BuiltinFunctionArguments<N>::type func) {
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
		return MatchSize(0); // FIXME; inspect _patt
	}
};

inline RuleRef make_rewrite_rule(const BaseExpressionRef &patt, const BaseExpressionRef &into) {
	return std::make_shared<RewriteRule>(patt, into);
}
