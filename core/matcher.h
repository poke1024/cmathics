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
	MatchId m_id;
	MatchNode *m_first;
	MatchNode *m_last;

public:
    inline VariableList(const MatchId &id) :
	    m_id(id), m_first(nullptr), m_last(nullptr) {
    }

    inline VariableList(const VariableList &list) :
	    m_id(list.m_id), m_first(list.m_first), m_last(list.m_last) {
    }

	inline const MatchId &id() const {
		return m_id;
	}

    inline bool is_empty() const {
        return m_first == nullptr;
    }

    inline void reset() {
	    m_first = nullptr;
	    m_last = nullptr;
    }

    inline MatchNode *get() const {
        return m_first;
    }

    inline bool assign(Symbol *variable, const BaseExpressionRef &value) {
	    VariableList * const self = this;

	    return variable->set_matched_value(m_id, value, [self] (MatchNode *node) {
		    MatchNode * const next = self->m_first;
		    node->next_in_list = self->m_first;
		    self->m_first = node;
		    if (next == nullptr) {
			    self->m_last = next;
		    }
	    });
    }

	inline void unassign(Symbol *variable) {
		VariableList * const self = this;

		variable->clear_matched_value(m_id, [self] (MatchNode *node) {
			assert(node == self->m_first);
			self->m_first = node->next_in_list;
			if (self->m_first == nullptr) {
				self->m_last = nullptr;
			}
		});
	}

    inline void prepend(VariableList &list) {
        MatchNode *list_first = list.m_first;
        if (list_first) {
            list.m_last->next_in_list = m_first;

            m_first = list_first;
	        if (!m_last) {
		        m_last = list.m_last;
	        }

	        list.m_first = nullptr;
	        list.m_last = nullptr;
        }
    }

	inline void backtrace(MatchNode *to_first) {
		while (m_first != to_first) {
			m_first = m_first->next_in_list;
		}
	}
};

typedef uint32_t MatchOptions;

class MatchContext {
public:
	enum {
		NoEndAnchor = 1
	};

	const Evaluation &evaluation;
    VariableList matched_variables;
	const MatchOptions options;

	inline MatchContext(const Evaluation &evaluation_, MatchOptions options_ = 0) :
        matched_variables(Heap::MatchId()), evaluation(evaluation_), options(options_) {
	}
};

class Match {
private:
	optional<VariableList> m_variables;

public:
	inline Match() {
	}

	inline Match(const Match &match) :
		m_variables(match.m_variables) {
	}

	inline Match(const MatchContext &context) :
		m_variables(context.matched_variables) {
	}

	inline operator bool() const {
		return bool(m_variables);
	}

	inline const MatchId &id() const {
		return m_variables->id();
	}

	inline MatchNode *variables() const {
		return m_variables->get();
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
	void operator()(const MatchNode *node, typename BaseExpressionTuple<M>::type &t) {
		// symbols are already ordered in the order of their (first) appearance in the original pattern.
		assert(node != nullptr);
		std::get<N>(t) = node->value;
		unpack_symbols<M, N + 1>()(node->next_in_list, t);
	}
};

template<int M>
struct unpack_symbols<M, M> {
	void operator()(const MatchNode *node, typename BaseExpressionTuple<M>::type &t) {
		assert(node == nullptr);
	}
};

template<int N>
inline typename BaseExpressionTuple<N>::type Match::get() const {
	typename BaseExpressionTuple<N>::type t;
	unpack_symbols<N, 0>()(m_variables->get(), t);
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
				MatchContext context(m_evaluation, MatchContext::NoEndAnchor);
				const index_t match_end = m_matcher->match(
					context, string, begin, end);
				if (match_end >= 0) {
					callback(begin, match_end, Match(std::move(context)));
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
				MatchContext context(m_evaluation, 0);
				callback(next, next + patt_string->length(), Match(std::move(context)));
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
		        return Match(std::move(context));
	        } else {
		        return Match();
	        }
        } else if (m_patt->type() == StringType) {
	        if (m_patt->as_string()->same(string)) {
		        MatchContext context(m_evaluation, 0);
		        return Match(std::move(context));
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
					FastLeafSequence(context, &item), 0, 1);
				if (match >= 0) {
					return Match(std::move(context));
				}
			}
			return Match(); // no match
		} else {
			if (m_patt->same(item)) {
				return Match(std::move(context));
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
