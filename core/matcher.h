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

typedef uint32_t MatchOptions;

class MatchContext {
public:
	enum {
		NoEndAnchor = 1
	};

	const MatchId id;
	const Evaluation &evaluation;
    VariableList matched_variables;
	const MatchOptions options;

	inline MatchContext(const Evaluation &evaluation_, MatchOptions options_ = 0) :
        id(Heap::MatchId()), evaluation(evaluation_), options(options_) {
	}
};

class Match {
private:
	const MatchId m_id;
	const Symbol *m_variables;

public:
	explicit inline Match() {
	}

	explicit inline Match(const MatchContext &context) :
		m_id(context.id), m_variables(context.matched_variables.get()) {
	}

	inline operator bool() const {
		return m_id.get() != nullptr;
	}

	inline const MatchId &id() const {
		return m_id;
	}

	inline const Symbol *variables() const {
		return m_variables;
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
	unpack_symbols<N, 0>()(m_variables, t);
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

class FastLeafSequence {
private:
	const Evaluation &m_evaluation;
	const BaseExpressionRef * const m_array;

public:
	class Element {
	private:
		const BaseExpressionRef * const m_begin;

	public:
		inline Element(const BaseExpressionRef *array, index_t begin) : m_begin(array + begin) {
		}

		inline const BaseExpressionRef &operator*() const {
			return *m_begin;
		}
	};

	class Sequence {
	private:
		const Evaluation &m_evaluation;
		const BaseExpressionRef * const m_begin;
		const index_t m_n;
		BaseExpressionRef m_sequence;

	public:
		inline Sequence(const Evaluation &evaluation, const BaseExpressionRef *array, index_t begin, index_t end) :
			m_evaluation(evaluation), m_begin(array + begin), m_n(end - begin) {
		}

		inline const BaseExpressionRef &operator*() {
			if (!m_sequence) {
				const BaseExpressionRef * const begin = m_begin;
				const index_t n = m_n;
				m_sequence = expression(m_evaluation.Sequence, [begin, n] (auto &storage) {
					for (index_t i = 0; i < n; i++) {
						storage << begin[i];
					}
				}, n);
			}
			return m_sequence;
		}
	};

	inline FastLeafSequence(const Evaluation& evaluation, const BaseExpressionRef *array) :
		m_evaluation(evaluation), m_array(array) {
	}

	inline Element element(index_t begin) const {
		return Element(m_array, begin);
	}

	inline Sequence sequence(index_t begin, index_t end) const {
        assert(begin <= end);
		return Sequence(m_evaluation, m_array, begin, end);
	}

	inline index_t same(index_t begin, BaseExpressionPtr other) const {
		const BaseExpressionPtr expr = m_array[begin].get();
		if (other == expr || other->same(expr)) {
			return begin + 1;
		} else {
			return -1;
		}
	}
};

class StringMatcher {
private:
    PatternMatcherRef m_matcher;
    const BaseExpressionRef m_patt;
    const Evaluation &m_evaluation;

public:
    inline StringMatcher(
        const BaseExpressionRef &patt,
        const Evaluation &evaluation) :

        m_patt(patt), m_evaluation(evaluation) {

        switch (patt->type()) {
            case ExpressionType:
                m_matcher = patt->as_expression()->string_matcher();
                break;

            case SymbolType:
                m_matcher = compile_string_pattern(patt); // FIXME
                break;

            default:
                break;
        }
    }

	template<typename Callback>
	inline void search(const String *string, const Callback &callback, bool overlap) const {
		if (m_matcher) {
			index_t begin = 0;
			const index_t end = string->length();
			while (begin < end) {
				MatchContext context(m_evaluation, MatchContext::NoEndAnchor);
				const index_t match_end = m_matcher->match(
					context, string, begin, end);
				if (match_end >= 0) {
					callback(begin, match_end, Match(context));
					if (overlap) {
						begin++;
					} else {
						begin = match_end;
					}
				} else {
					begin++;
				}
			}
		} else if (m_patt->type() == StringType) {
			MatchContext context(m_evaluation, 0);
			index_t curr = 0;
			const String *patt_string = m_patt->as_string();
			const auto string_unicode = string->unicode();
			const auto patt_unicode = patt_string->unicode();
			while (true) {
				const index_t next = string_unicode.indexOf(patt_unicode, curr);
				if (next < 0) {
					break;
				}
				callback(next, next + patt_string->length(), Match(context));
				curr = next + 1;
			}
		}
	}

    inline Match operator()(const String *string) const {
        if (m_matcher) {
	        MatchContext context(m_evaluation, 0);
	        const index_t match_end = m_matcher->match(
			    context, string, 0, string->length());
	        if (match_end >= 0) {
		        return Match(context);
	        } else {
		        return Match();
	        }
        } else if (m_patt->type() == StringType) {
	        if (m_patt->as_string()->same(string)) {
		        MatchContext context(m_evaluation, 0);
		        return Match(context);
	        } else {
		        return Match();
	        }
        } else {
            return Match();
        }
    }
};

class Matcher {
private:
	PatternMatcherRef m_matcher;
	const BaseExpressionRef m_patt;
	const Evaluation &m_evaluation;

public:
	inline Matcher(const BaseExpressionRef &patt, const Evaluation &evaluation) :
		m_patt(patt), m_evaluation(evaluation) {

		if (patt->type() == ExpressionType) {
			m_matcher = patt->as_expression()->expression_matcher();
		}
	}

	inline Match operator()(const BaseExpressionRef &item) const {
		MatchContext context(m_evaluation);
		if (m_matcher) {
			if (m_matcher->might_match(1)) {
				const index_t match = m_matcher->match(
					context, FastLeafSequence(m_evaluation, &item), 0, 1);
				if (match >= 0) {
					return Match(context);
				}
			}
			return Match(); // no match
		} else {
			if (m_patt->same(item)) {
				return Match(context);
			} else {
				return Match(); // no match
			}
		}
	}
};

inline Match match(const BaseExpressionRef &patt, const BaseExpressionRef &item, const Evaluation &evaluation) {
	const Matcher matcher(patt, evaluation);
	return matcher(item);
}

#endif //CMATHICS_MATCHER_H
