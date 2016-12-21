#ifndef CMATHICS_MATCHER_H
#define CMATHICS_MATCHER_H

#include <assert.h>

#include "types.h"
#include "evaluation.h"
#include "symbol.h"
#include "expression.h"

PatternMatcherRef compile_expression_pattern(const BaseExpressionRef &patt);

PatternMatcherRef compile_string_pattern(const BaseExpressionRef &patt);

typedef uint32_t MatchOptions;

class MatchContext {
public:
	enum {
		NoEndAnchor = 1
	};

	const Evaluation &evaluation;
	MatchRef match;
	const MatchOptions options;

	inline MatchContext(
		const PatternMatcherRef &matcher,
		const Evaluation &evaluation_,
		MatchOptions options_ = 0) :

        evaluation(evaluation_), match(Heap::Match(matcher)), options(options_) {
	}

	inline void reset() {
		match->reset();
	}
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
	void operator()(
		const Match &match,
		const index_t index,
		typename BaseExpressionTuple<M>::type &t) {

		// symbols are already ordered in the order of their (first) appearance in the original pattern.
		assert(index != match.n_slots_fixed());
		std::get<N>(t) = match.ith_slot(index);
		unpack_symbols<M, N + 1>()(match, index + 1, t);
	}
};

template<int M>
struct unpack_symbols<M, M> {
	void operator()(
		const Match &match,
		const index_t index,
		typename BaseExpressionTuple<M>::type &t) {

		assert(index == match.n_slots_fixed());
	}
};

template<int N>
inline typename BaseExpressionTuple<N>::type Match::unpack() const {
	typename BaseExpressionTuple<N>::type t;
	unpack_symbols<N, 0>()(*this, 0, t);
	return t;
};

#include "leaves.h"

inline PatternMatcherRef Expression::expression_matcher() const { // concurrent.
	const CacheRef cache = ensure_cache();
	PatternMatcherRef matcher = cache->expression_matcher;
	if (!matcher) {
		matcher = compile_expression_pattern(BaseExpressionRef(this));
		cache->expression_matcher = matcher;
	}
	return matcher;
}

inline PatternMatcherRef Expression::string_matcher() const { // concurrent.
	const CacheRef cache = ensure_cache();
	PatternMatcherRef matcher = cache->string_matcher;
	if (!matcher) {
		matcher = compile_string_pattern(BaseExpressionRef(this));
		cache->string_matcher = matcher;
	}
	return matcher;
}

class FastLeafSequence {
private:
	MatchContext &m_context;
	const BaseExpressionRef * const m_array;

public:
	class Element {
	private:
		const BaseExpressionRef * const m_array;
		const index_t m_begin;

	public:
		inline Element(const BaseExpressionRef *array, index_t begin) : m_array(array), m_begin(begin) {
		}

		inline index_t begin() const {
			return m_begin;
		}

		inline const BaseExpressionRef &operator*() const {
			return m_array[m_begin];
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

	inline FastLeafSequence(MatchContext &context, const BaseExpressionRef *array) :
		m_context(context), m_array(array) {
	}

	inline MatchContext &context() const {
		return m_context;
	}

	inline Element element(index_t begin) const {
		return Element(m_array, begin);
	}

	inline Sequence slice(index_t begin, index_t end) const {
        assert(begin <= end);
		return Sequence(m_context.evaluation, m_array, begin, end);
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
			MatchContext context(m_matcher, m_evaluation, MatchContext::NoEndAnchor);
			while (begin < end) {
				const index_t match_end = m_matcher->match(
					context, string, begin, end);
				if (match_end >= 0) {
					callback(begin, match_end, context.match);
					if (overlap) {
						begin++;
					} else {
						begin = match_end;
					}
					if (begin < end) {
						context.reset();
					}
				} else {
					begin++;
				}
			}
		} else if (m_patt->type() == StringType) {
			index_t curr = 0;

			const String *patt_string = m_patt->as_string();
			const auto string_unicode = string->unicode();
			const auto patt_unicode = patt_string->unicode();
			const auto match = Heap::DefaultMatch();

			while (true) {
				const index_t next = string_unicode.indexOf(patt_unicode, curr);
				if (next < 0) {
					break;
				}
				callback(next, next + patt_string->length(), match);
				curr = next + 1;
			}
		}
	}

    inline MatchRef operator()(const String *string) const {
        if (m_matcher) {
	        MatchContext context(m_matcher, m_evaluation, 0);
	        const index_t match_end = m_matcher->match(
			    context, string, 0, string->length());
	        if (match_end >= 0) {
		        return context.match;
	        } else {
		        return MatchRef();
	        }
        } else if (m_patt->type() == StringType) {
	        if (m_patt->as_string()->same(string)) {
		        return Heap::DefaultMatch();
	        } else {
		        return MatchRef();
	        }
        } else {
            return MatchRef();
        }
    }
};

class Matcher {
private:
	typedef MatchRef (Matcher::*MatchFunction)(const BaseExpressionRef &, const Evaluation &) const;

	PatternMatcherRef m_matcher;
	const BaseExpressionRef m_patt;
	MatchFunction m_perform_match;

private:
	inline MatchRef match_atom(const BaseExpressionRef &item, const Evaluation &evaluation) const {
		if (m_patt->same(item)) {
			return Heap::DefaultMatch();
		} else {
			return MatchRef(); // no match
		}
	}

	inline MatchRef match_expression(const BaseExpressionRef &item, const Evaluation &evaluation) const {
		MatchContext context(m_matcher, evaluation);
		const index_t match = m_matcher->match(
			FastLeafSequence(context, &item), 0, 1);
		if (match >= 0) {
			return context.match;
		} else {
			return MatchRef(); // no match
		}
	}

	inline MatchRef match_none(const BaseExpressionRef &item, const Evaluation &evaluation) const {
		return MatchRef(); // no match
	}

public:
	inline Matcher(const BaseExpressionRef &patt) :
		m_patt(patt) {

		if (patt->type() == ExpressionType) {
			m_matcher = patt->as_expression()->expression_matcher();
			if (m_matcher->might_match(1)) {
				m_perform_match = &Matcher::match_expression;
			} else {
				m_perform_match = &Matcher::match_none;
			}
		} else {
			m_perform_match = &Matcher::match_atom;
		}
	}

	inline MatchRef operator()(const BaseExpressionRef &item, const Evaluation &evaluation) const {
		return (this->*m_perform_match)(item, evaluation);
	}

	FunctionBodyRef precompile(const BaseExpressionRef &item) const;
};

template<typename F>
inline auto match(const BaseExpressionRef &patt, const Evaluation &evaluation, const F &f) {
	if (patt->type() == ExpressionType) {
		const auto matcher = patt->as_expression()->expression_matcher();
		if (matcher->might_match(1)) {
			MatchContext context(matcher, evaluation);
			return f([&matcher, &context] (const BaseExpressionRef &item) {
				context.reset();
				const index_t match = matcher->match(
					FastLeafSequence(context, &item), 0, 1);
				if (match >= 0) {
					return context.match;
				} else {
					return MatchRef(); // no match
				}
			});
		} else {
			return f([] (const BaseExpressionRef &item) {
				return MatchRef(); // no match
			});
		}
	} else {
		return f([&patt] (const BaseExpressionRef &item) {
			if (patt->same(item)) {
				return Heap::DefaultMatch();
			} else {
				return MatchRef(); // no match
			}
		});
	}
}

#endif //CMATHICS_MATCHER_H
