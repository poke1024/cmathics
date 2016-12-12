#ifndef CMATHICS_MATCHER_H
#define CMATHICS_MATCHER_H

#include <assert.h>

#include "types.h"
#include "evaluation.h"
#include "symbol.h"
#include "expression.h"

PatternMatcherRef compile_expression_pattern(const BaseExpressionRef &patt);

PatternMatcherRef compile_string_pattern(const BaseExpressionRef &patt);

class VariableList {
private:
    Symbol *_first;
    Symbol *_last;

public:
    inline VariableList() : _first(nullptr), _last(nullptr) {
    }

    inline VariableList(const VariableList &list) : _first(list._first), _last(list._last) {
    }

    inline bool is_empty() const {
        return _first == nullptr;
    }

    inline void reset() {
        _first = nullptr;
        _last = nullptr;
    }

    inline Symbol *get() const {
        return _first;
    }

    inline void prepend(Symbol *symbol) {
        Symbol *old_first = _first;
        if (old_first) {
            symbol->set_next_variable(old_first);
            _first = symbol;
        } else {
            symbol->set_next_variable(nullptr);
            _first = symbol;
            _last = symbol;
        }
    }

    inline void prepend(const VariableList &list) {
        Symbol *list_first = list._first;
        if (list_first) {
            list._last->set_next_variable(_first);
            _first = list_first;
        }
    }
};

class MatchContext {
public:
	enum Anchor {
		AnchoredEnd,
		Unanchored
	};

	const MatchId id;
	Definitions &definitions;
    VariableList matched_variables;
	Anchor anchor;

	inline MatchContext(const BaseExpressionRef &patt, const BaseExpressionRef &item, Definitions &defs, Anchor anch) :
        id(patt, item), definitions(defs), anchor(anch) {
	}
};

class Match {
private:
	const bool _matched;
	const MatchId _id;
	const Symbol *_variables;

public:
	explicit inline Match() : _matched(false) {
	}

	explicit inline Match(bool matched, const MatchContext &context) :
		_matched(matched), _id(context.id), _variables(context.matched_variables.get()) {
	}

	inline operator bool() const {
		return _matched;
	}

	inline const MatchId &id() const {
		return _id;
	}

	inline const Symbol *variables() const {
		return _variables;
	}

	template<int N>
	typename BaseExpressionTuple<N>::type get() const;
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

template<int M, int N>
struct unpack_symbols {
	void operator()(const Symbol *symbol, typename BaseExpressionTuple<M>::type &t) {
		// symbols are already ordered in the order of their (first) appearance in the original pattern.
		assert(symbol != nullptr);
		std::get<N>(t) = symbol->matched_value();
		unpack_symbols<M, N + 1>()(symbol->next_variable(), t);
	}
};

template<int M>
struct unpack_symbols<M, M> {
	void operator()(const Symbol *symbol, typename BaseExpressionTuple<M>::type &t) {
		assert(symbol == nullptr);
	}
};

template<int N>
inline typename BaseExpressionTuple<N>::type Match::get() const {
	typename BaseExpressionTuple<N>::type t;
	unpack_symbols<N, 0>()(_variables, t);
	return t;
};

#include "leaves.h"

inline PatternMatcherRef Expression::expression_matcher() const {
	Cache * const cache_ptr = cache();
	if (!cache_ptr->expression_matcher) {
		cache_ptr->expression_matcher = compile_expression_pattern(BaseExpressionRef(this));
	}
	return cache_ptr->expression_matcher;
}

inline PatternMatcherRef Expression::string_matcher() const {
	Cache *const cache_ptr = cache();
	if (!cache_ptr->string_matcher) {
		cache_ptr->string_matcher = compile_string_pattern(BaseExpressionRef(this));
	}
	return cache_ptr->string_matcher;
}

inline Match match(const BaseExpressionRef &patt, const BaseExpressionRef &item, Definitions &definitions) {
	MatchContext context(patt, item, definitions, MatchContext::AnchoredEnd);
	if (patt->type() != ExpressionType) {
		if (patt->same(item)) {
			return Match(true, context);
		} else {
			return Match(); // no match
		}
	} else {
		const auto &matcher = patt->as_expression()->expression_matcher();
		if (matcher->might_match(1) && matcher->match(context, &item, &item + 1)) {
			return Match(true, context);
		} else {
			return Match(); // no match
		}
	}
}

#endif //CMATHICS_MATCHER_H
