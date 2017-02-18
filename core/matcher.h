#ifndef CMATHICS_MATCHER_H
#define CMATHICS_MATCHER_H

#include <assert.h>

#include "types.h"
#include "evaluation.h"
#include "symbol.h"
#include "expression.h"
#include "builtin.h"

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

        evaluation(evaluation_),
        match(Pool::Match(matcher)),
        options(options_) {
	}

    inline MatchContext(
        const PatternMatcherRef &matcher,
        const nothing&,
        const Evaluation &evaluation,
        MatchOptions options = 0) :

        MatchContext(matcher, evaluation, options) {
    }

    inline MatchContext(
        const PatternMatcherRef &matcher,
        const OptionsProcessorRef &options_processor,
        const Evaluation &evaluation_,
        MatchOptions options_ = 0) :

        evaluation(evaluation_),
        match(Pool::Match(matcher, options_processor)),
        options(options_) {
    }

	inline void reset() {
		match->reset();
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
		std::get<N>(t) = match.ith_slot(index).get();
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
	return ensure_cache()->expression_matcher(this);
}

inline PatternMatcherRef Expression::string_matcher() const { // concurrent.
	return ensure_cache()->string_matcher(this);
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
		UnsafeBaseExpressionRef m_sequence;

	public:
		inline Sequence(const Evaluation &evaluation, const BaseExpressionRef *array, index_t begin, index_t end) :
			m_evaluation(evaluation), m_begin(array + begin), m_n(end - begin) {
		}

		inline const UnsafeBaseExpressionRef &operator*() {
			if (!m_sequence) {
				const BaseExpressionRef * const begin = m_begin;
				const index_t n = m_n;
				m_sequence = expression(m_evaluation.Sequence, sequential([begin, n] (auto &store) {
					for (index_t i = 0; i < n; i++) {
						store(BaseExpressionRef(begin[i]));
					}
				}, n));
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

class SlowLeafSequence {
private:
    MatchContext &m_context;
    const Expression * const m_expr;

public:
    class Element {
    private:
        const Expression * const m_expr;
        const index_t m_begin;
	    UnsafeBaseExpressionRef m_element;

    public:
        inline Element(const Expression *expr, index_t begin) : m_expr(expr), m_begin(begin) {
        }

        inline index_t begin() const {
            return m_begin;
        }

        inline const UnsafeBaseExpressionRef &operator*() {
            if (!m_element) {
                m_element = m_expr->materialize_leaf(m_begin);
            }
            return m_element;
        }
    };

    class Sequence {
    private:
        const Evaluation &m_evaluation;
        const Expression * const m_expr;
        const index_t m_begin;
        const index_t m_end;
	    UnsafeBaseExpressionRef m_sequence;

    public:
        inline Sequence(const Evaluation &evaluation, const Expression *expr, index_t begin, index_t end) :
            m_evaluation(evaluation), m_expr(expr), m_begin(begin), m_end(end) {
        }

        inline const UnsafeBaseExpressionRef &operator*() {
            if (!m_sequence) {
                m_sequence = m_expr->slice(m_evaluation.Sequence, m_begin, m_end);
            }
            return m_sequence;
        }
    };

    inline SlowLeafSequence(MatchContext &context, const Expression *expr) :
        m_context(context), m_expr(expr) {
    }

    inline MatchContext &context() const {
        return m_context;
    }

    inline Element element(index_t begin) const {
        return Element(m_expr, begin);
    }

    inline Sequence slice(index_t begin, index_t end) const {
        assert(begin <= end);
        return Sequence(m_context.evaluation, m_expr, begin, end);
    }

    inline index_t same(index_t begin, BaseExpressionPtr other) const {
        if (other->same(m_expr->materialize_leaf(begin))) {
            return begin + 1;
        } else {
            return -1;
        }
    }
};

class StringMatcher {
private:
    CachedPatternMatcherRef m_matcher;
    const BaseExpressionRef m_patt;
    const Evaluation &m_evaluation;

public:
    inline StringMatcher(
        const BaseExpressionRef &patt,
        const Evaluation &evaluation) :

        m_patt(patt), m_evaluation(evaluation) {

        switch (patt->type()) {
            case ExpressionType:
                m_matcher.initialize(patt->as_expression()->string_matcher());
                break;

            case SymbolType:
                m_matcher.initialize(compile_string_pattern(patt)); // FIXME
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
			const auto match = Pool::DefaultMatch();

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
		        return Pool::DefaultMatch();
	        } else {
		        return MatchRef();
	        }
        } else {
            return MatchRef();
        }
    }
};

class HeadLeavesMatcher {
private:
    const PatternMatcherRef m_match_head;
    const PatternMatcherRef m_match_leaves;

    template<bool MatchHead>
    inline bool match(
        MatchContext &context,
        const Expression *expr) const {

        const PatternMatcherRef &match_leaves = m_match_leaves;

        if (!match_leaves->might_match(expr->size())) {
            return false;
        }

        if (MatchHead) {
            const BaseExpressionRef &head = expr->head();

            if (m_match_head->match(FastLeafSequence(context, &head), 0, 1) < 0) {
                return false;
            }
        }

        if (slice_needs_no_materialize(expr->slice_code())) {
            if (expr->with_leaves_array([&context, &match_leaves] (const BaseExpressionRef *leaves, size_t size) {
                return match_leaves->match(FastLeafSequence(context, leaves), 0, size);
            }) < 0) {
                return false;
            }
        } else {
            if (match_leaves->match(SlowLeafSequence(context, expr), 0, expr->size()) < 0) {
                return false;
            }
        }

        return true;
    }

public:
    inline HeadLeavesMatcher(
        const PatternMatcherRef &match_head,
        const PatternMatcherRef &match_leaves) :

        m_match_head(match_head),
        m_match_leaves(match_leaves) {
    }

    inline std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "HeadLeavesMatcher(" << m_match_head->name(context) << ", ";
        s << m_match_leaves->name(context) << ")";
        return s.str();
    }

    inline bool with_head(
        MatchContext &context,
        const Expression *expr) const {
        return match<true>(context, expr);
    }

    inline bool without_head(
        MatchContext &context,
        const Expression *expr) const {
        return match<false>(context, expr);
    }
};

class MatcherBase {
protected:
	CachedPatternMatcherRef m_matcher;

public:
	RewriteBaseExpression prepare(const BaseExpressionRef &item) const;
};

template<typename OptionsProcessor>
class OptionsMatcher : public MatcherBase {
private:
    typedef MatchRef (OptionsMatcher::*MatchFunction)(
        const BaseExpressionRef &, const OptionsProcessor &, const Evaluation &) const;

    const BaseExpressionRef m_patt;
    MatchFunction m_perform_match;

private:
    MatchRef match_atom(
        const BaseExpressionRef &item, const OptionsProcessor &options, const Evaluation &evaluation) const {

        if (m_patt->same(item)) {
            return Pool::DefaultMatch();
        } else {
            return MatchRef(); // no match
        }
    }

    MatchRef match_expression(
        const BaseExpressionRef &item, const OptionsProcessor &options, const Evaluation &evaluation) const {

        MatchContext context(m_matcher, options, evaluation);
        const index_t match = m_matcher->match(
            FastLeafSequence(context, &item), 0, 1);
        if (match >= 0) {
            return context.match;
        } else {
            return MatchRef(); // no match
        }
    }

    MatchRef match_none(
        const BaseExpressionRef &item, const OptionsProcessor &options, const Evaluation &evaluation) const {

        return MatchRef(); // no match
    }

public:
    inline OptionsMatcher(const BaseExpressionRef &patt) :
        m_patt(patt) {

        if (patt->type() == ExpressionType) {
            m_matcher.initialize(patt->as_expression()->expression_matcher());
            if (m_matcher->might_match(1)) {
                m_perform_match = &OptionsMatcher<OptionsProcessor>::match_expression;
            } else {
                m_perform_match = &OptionsMatcher<OptionsProcessor>::match_none;
            }
        } else {
            m_perform_match = &OptionsMatcher<OptionsProcessor>::match_atom;
        }
    }

    inline MatchRef operator()(
        const BaseExpressionRef &item, const OptionsProcessor &options, const Evaluation &evaluation) const {

        return (this->*m_perform_match)(item, options, evaluation);
    }
};

class Matcher : public OptionsMatcher<nothing> {
public:
    using OptionsMatcher<nothing>::OptionsMatcher;

    inline MatchRef operator()(
        const BaseExpressionRef &item, const Evaluation &evaluation) const {

        return OptionsMatcher<nothing>::operator()(item, nothing(), evaluation);
    }
};

// A SequenceMatcher is a Matcher that takes an Expression and matches only the leaves
// but not the head - it's basically a Matcher that ignores the head and is thus faster.

class SequenceMatcher : public MatcherBase {
private:
    const HeadLeavesMatcher *m_head_leaves_matcher;

public:
    inline SequenceMatcher(const BaseExpressionRef &patt) : m_head_leaves_matcher(nullptr) {
        if (patt->type() == ExpressionType) {
            const PatternMatcherRef matcher =
                patt->as_expression()->expression_matcher();
            const auto head_leaves_matcher = matcher->head_leaves_matcher();
            if (head_leaves_matcher) {
	            if (matcher->might_match(1)) {
		            m_matcher.initialize(matcher); // validates ptr to m_head_leaves_matcher
		            m_head_leaves_matcher = head_leaves_matcher;
	            } else {
		            m_head_leaves_matcher = nullptr;
	            }
            } else {
	            throw std::runtime_error("constructed a SequenceMatcher for a non-expression pattern");
            }
        } else {
	        throw std::runtime_error("constructed a SequenceMatcher for a non-expression pattern");
        }
    }

    inline MatchRef operator()(const Expression *expr, const Evaluation &evaluation) const {
	    const auto * const matcher = m_head_leaves_matcher;
	    if (!matcher) {
		    return MatchRef();
	    }
        MatchContext context(m_matcher, evaluation);
        if (matcher->without_head(context, expr)) {
            return context.match;
        } else {
            return MatchRef();
        }
    }
};

template<typename Rule, typename F>
inline auto match(
    const Rule &rule,
    const F &f,
	const Expression *cache_owner,
    const Evaluation &evaluation) {

    constexpr int has_rhs = std::tuple_size<Rule>::value > 1;
    const BaseExpressionRef &lhs = std::get<0>(rule);
    const BaseExpressionRef &rhs = std::get<std::tuple_size<Rule>::value - 1>(rule);

	if (lhs->type() == ExpressionType) {
		const auto matcher = lhs->as_expression()->expression_matcher();
		if (matcher->might_match(1)) {
			MatchContext context(matcher, evaluation);

            const auto make = [&f, &matcher, &context] (const auto &extract) {
                return f([&matcher, &context, &extract] (const BaseExpressionRef &item) {
                    context.reset();
                    const index_t match = matcher->match(
                        FastLeafSequence(context, &item), 0, 1);
                    if (match >= 0) {
                        return extract(item);
                    } else {
                        return BaseExpressionRef(); // no match
                    }
                });
            };

            if (has_rhs) {
	            const RewriteRef rewrite = cache_owner->ensure_cache()->rewrite(matcher, rhs);

                return make([&rewrite, &context, &rhs] (const BaseExpressionRef &item) {
                    return rewrite->rewrite_root_or_copy(
                        rhs->as_expression(),
                        [&context] (index_t i, const BaseExpressionRef &prev) {
                            return context.match->slot(i);
                        },
                        context.match->options());
                });
            } else {
                return make([] (const BaseExpressionRef &item) {
                    return item;
                });
            }
		} else {
			return f([] (const BaseExpressionRef &item) {
				return BaseExpressionRef(); // no match
			});
		}
	} else {
		return f([&lhs, &rhs, &has_rhs] (const BaseExpressionRef &item) {
			if (lhs->same(item)) {
				return has_rhs ? rhs : item;
			} else {
				return BaseExpressionRef(); // no match
			}
		});
	}
}

typedef RewriteRule<Matcher> SubRule;

typedef RewriteRule<Matcher> UpRule;

// DownRule assumes that the expression's head was matched already using the down lookup rule
// process, so it only looks at the leaves.

typedef RewriteRule<SequenceMatcher> DownRule;

inline NewRuleRef make_down_rule(const BaseExpressionRef &patt, const BaseExpressionRef &into) {
	return [patt, into] (const SymbolRef &head, const Definitions &definitions) -> RuleRef {
		return RuleRef(new DownRule(patt, into));
	};
}

// note: PatternMatchedBuiltinRule should only be used for builtins that match non-down values (e.g. sub values).
// if you write builtins that match down values, always use BuiltinRule, since it's faster (it doesn't involve the
// pattern match).

template<int N, typename F>
class PatternMatchedBuiltinRule : public Rule {
private:
    const F function;
    const Matcher matcher;

public:
    inline PatternMatchedBuiltinRule(const BaseExpressionRef &patt, const F &f) :
        Rule(patt), function(f), matcher(pattern) {
    }

    virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

        const MatchRef match = matcher(expr, evaluation);
        if (match) {
            assert(match->n_slots_fixed() == N);
            return apply_from_tuple(function, std::tuple_cat(
                std::forward_as_tuple(expr),
                match->unpack<N>(),
                std::forward_as_tuple(evaluation)));
        } else {
            return optional<BaseExpressionRef>();
        }
    }

    virtual MatchSize leaf_match_size() const {
        assert(pattern->type() == ExpressionType);
        return pattern->as_expression()->leaf_match_size();
    }

    virtual SortKey pattern_key() const {
        return pattern->pattern_key();
    }
};

template<int N, typename F>
class PatternMatchedOptionsBuiltinRule : public Rule {
private:
	const F function;
	const Matcher matcher;

public:
	inline PatternMatchedOptionsBuiltinRule(const BaseExpressionRef &patt, const F &f) :
		Rule(patt), function(f), matcher(pattern) {
	}

	virtual optional<BaseExpressionRef> try_apply(
		const Expression *expr,
		const Evaluation &evaluation) const {

		const MatchRef match = matcher(expr, evaluation);
		if (match) {
			assert(match->n_slots_fixed() == N);
			return apply_from_tuple(function, std::tuple_cat(
				std::forward_as_tuple(expr),
				match->unpack<N>(),
				std::forward_as_tuple(*match->options()), // FIXME
				std::forward_as_tuple(evaluation)));
		} else {
			return optional<BaseExpressionRef>();
		}
	}

	virtual MatchSize leaf_match_size() const {
		assert(pattern->type() == ExpressionType);
		return pattern->as_expression()->leaf_match_size();
	}

	virtual SortKey pattern_key() const {
		return pattern->pattern_key();
	}
};

template<int N>
inline NewRuleRef make_pattern_matched_builtin_rule(
    const BaseExpressionRef &patt, typename BuiltinFunctionArguments<N>::type func) {

    return [patt, func] (const SymbolRef &head, const Definitions &definitions) -> RuleRef {
        return new PatternMatchedBuiltinRule<N, decltype(func)>(patt, func);
    };
}

template<typename Assign>
bool parse_options(
	const Assign &assign,
	const BaseExpressionRef &item,
	const Evaluation &evaluation) {

	if (!item->is_expression()) {
		return false;
	}

	const Expression * const expr = item->as_expression();

	switch (expr->head()->symbol()) {
		case S::List:
			return expr->with_slice([&assign, &evaluation] (const auto &slice) {
				const size_t n = slice.size();
				for (size_t i = 0; i < n; i++) {
					if (!parse_options(assign, slice[i], evaluation)) {
						return false;
					}
				}
				return true;
			});

		case S::Rule:
		case S::RuleDelayed:
			if (expr->size() == 2) {
				const auto * const leaves = expr->n_leaves<2>();
				UnsafeSymbolRef name;

				const auto &lhs = leaves[0];
				if (lhs->is_symbol()) {
					name = lhs->as_symbol();
				} else if (lhs->is_string()) {
						name = lhs->as_string()->option_symbol(evaluation);
						if (!name) {
							return false;
						}
					} else {
						return false;
					}

				const auto &rhs = leaves[1];
				assign(name.get(), rhs);

				return true;
			}
			return false;

		default:
			return false;
	}
}

template<typename Sequence, typename Assign, typename Rollback>
index_t OptionsProcessor::parse(
	const Sequence &sequence,
	index_t begin,
	index_t end,
	const Assign &assign,
	const Rollback &rollback,
	const MatchRest &rest) {

	index_t t = begin;
	while (t < end) {
		if (!parse_options(
			assign,
			*sequence.element(t),
			sequence.context().evaluation)) {

			break;
		}
		t++;
	}

	const index_t m = rest(begin, t, end);

	if (t > begin && m < 0) {
        rollback();
	}

	return m;
}

template<typename Sequence>
inline index_t DynamicOptionsProcessor::do_match(
	const Sequence &sequence,
	index_t begin,
	index_t end,
	const MatchRest &rest) {

	OptionsMap options(
		m_options,
		Pool::options_map_allocator());

    std::swap(options, m_options);

	return parse(sequence, begin, end,
		[this] (SymbolPtr name, const BaseExpressionRef &value) {
			m_options[name] = value;
		},
		[this, &options] () {
			std::swap(options, m_options);
		},
		rest);
}

template<typename Options>
index_t StaticOptionsProcessor<Options>::match(
    const SlowLeafSequence &sequence,
    index_t begin,
    index_t end,
    const MatchRest &rest) {

    return do_match(sequence, begin, end, rest);
}

template<typename Options>
index_t StaticOptionsProcessor<Options>::match(
    const FastLeafSequence &sequence,
    index_t begin,
    index_t end,
    const MatchRest &rest) {

    return do_match(sequence, begin, end, rest);
}

template<typename Options>
template<typename Sequence>
inline index_t StaticOptionsProcessor<Options>::do_match(
    const Sequence &sequence,
    index_t begin,
    index_t end,
    const MatchRest &rest) {

	optional<Options> saved_options;

	if (!m_initialized) {
		m_controller.initialize_defaults(m_options);
		m_initialized = true;
	} else if (!m_clean) {
		saved_options = m_options;
	}

	m_clean = false;

	return parse(sequence, begin, end,
		[this] (SymbolPtr name, const BaseExpressionRef &value) {
			m_controller.set(m_options, name, value);
		},
		[this, &saved_options] () {
			if (!saved_options) {
				m_controller.initialize_defaults(m_options);
				m_initialized = true;
				m_clean = true;
			} else {
				m_options = *saved_options;
			}
		}, rest);
}

template<typename Sequence>
inline index_t Match::options(
    const Sequence &sequence,
    index_t begin,
    index_t end,
    const OptionsProcessor::MatchRest &rest) {

    if (!m_options) {
        m_options = Pool::DynamicOptionsProcessor();
    }

    return m_options->match(sequence, begin, end, rest);
}

inline const OptionsMap *Match::options() const {
    if (!m_options) {
        return nullptr;
    }
    if (m_options.get()->type == OptionsProcessor::Dynamic) {
        return &static_cast<DynamicOptionsProcessor*>(m_options.get())->options();
    } else {
        return nullptr;
    }
}

#endif //CMATHICS_MATCHER_H
