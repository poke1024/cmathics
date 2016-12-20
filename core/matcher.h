#ifndef CMATHICS_MATCHER_H
#define CMATHICS_MATCHER_H

#include <assert.h>

#include "types.h"
#include "evaluation.h"
#include "symbol.h"
#include "expression.h"

PatternMatcherRef compile_expression_pattern(const BaseExpressionRef &patt);

PatternMatcherRef compile_string_pattern(const BaseExpressionRef &patt);

class Match {
private:
	bool m_success;
	PatternMatcherRef m_matcher;
	std::vector<Slot, SlotAllocator> m_slots;
	index_t m_slots_fixed;

public:
    inline Match(bool success = false) : m_success(success) {
	    m_slots_fixed = 0;
    }

	inline Match(const PatternMatcherRef &matcher) :
		m_success(true),
		m_matcher(matcher),
		m_slots(matcher->variables().size(), Heap::slots_allocator()),
		m_slots_fixed(0) {
	}

    inline Match(Match &&match) :
	    m_success(match.m_success),
	    m_matcher(std::move(match.m_matcher)),

	    m_slots(std::move(match.m_slots)),
	    m_slots_fixed(match.m_slots_fixed) {
    }

	inline operator bool() const {
		return m_success;
	}

	inline const BaseExpressionRef *get_matched_value(const Symbol *variable) const {
		const index_t index = m_matcher->variables().find(variable);
		if (index >= 0) {
			return &m_slots[index].value;
		} else {
			return nullptr;
		}
	}

    inline bool assign(const index_t slot_index, const BaseExpressionRef &value) {
	    Slot &slot = m_slots[slot_index];
	    if (slot.value) {
		    return slot.value->same(value);
	    } else {
		    slot.value = value;
		    m_slots[m_slots_fixed++].index_to_ith = slot_index;
		    return true;
	    }
    }

	inline void unassign(const index_t slot_index) {
		m_slots_fixed--;
		assert(m_slots[m_slots_fixed].index_to_ith == slot_index);
		m_slots[slot_index].value.reset();
	}

    inline void prepend(Match &match) {
	    const index_t k = m_slots_fixed;
	    const index_t n = match.m_slots_fixed;

	    for (index_t i = 0; i < k; i++) {
		    m_slots[i + n].index_to_ith = m_slots[i].index_to_ith;
	    }

	    for (index_t i = 0; i < n; i++) {
		    const index_t index = match.m_slots[i].index_to_ith;
		    m_slots[i].index_to_ith = index;
		    m_slots[index].value = match.m_slots[index].value;
	    }

	    m_slots_fixed = n + k;
    }

	inline void backtrack(index_t n) {
		while (m_slots_fixed > n) {
			m_slots_fixed--;
			const index_t index = m_slots[m_slots_fixed].index_to_ith;
			m_slots[index].value.reset();
		}
	}

	inline size_t n_slots_fixed() const {
		return m_slots_fixed;
	}

	inline BaseExpressionRef slot(index_t i) const {
		return m_slots[m_slots[i].index_to_ith].value;
	}

	template<int N>
	typename BaseExpressionTuple<N>::type unpack() const;
};

typedef uint32_t MatchOptions;

class MatchContext {
public:
	enum {
		NoEndAnchor = 1
	};

	const Evaluation &evaluation;
    Match match;
	const MatchOptions options;

	inline MatchContext(
		const PatternMatcherRef &matcher,
		const Evaluation &evaluation_,
		MatchOptions options_ = 0) :

        evaluation(evaluation_), match(matcher), options(options_) {
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
		std::get<N>(t) = match.slot(index);
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
			while (begin < end) {
				MatchContext context(m_matcher, m_evaluation, MatchContext::NoEndAnchor);
				const index_t match_end = m_matcher->match(
					context, string, begin, end);
				if (match_end >= 0) {
					callback(begin, match_end, context.match);
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
			index_t curr = 0;

			const String *patt_string = m_patt->as_string();
			const auto string_unicode = string->unicode();
			const auto patt_unicode = patt_string->unicode();

			while (true) {
				const index_t next = string_unicode.indexOf(patt_unicode, curr);
				if (next < 0) {
					break;
				}
				callback(next, next + patt_string->length(), Match(true));
				curr = next + 1;
			}
		}
	}

    inline Match operator()(const String *string) const {
        if (m_matcher) {
	        MatchContext context(m_matcher, m_evaluation, 0);
	        const index_t match_end = m_matcher->match(
			    context, string, 0, string->length());
	        if (match_end >= 0) {
		        return std::move(context.match);
	        } else {
		        return Match();
	        }
        } else if (m_patt->type() == StringType) {
	        if (m_patt->as_string()->same(string)) {
		        return Match(true);
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
		if (m_matcher) {
			if (m_matcher->might_match(1)) {
				MatchContext context(m_matcher, m_evaluation);
				const index_t match = m_matcher->match(
					FastLeafSequence(context, &item), 0, 1);
				if (match >= 0) {
					return std::move(context.match);
				}
			}
			return Match(); // no match
		} else {
			if (m_patt->same(item)) {
				return Match(true);
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
