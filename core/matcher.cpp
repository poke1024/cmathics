#include "types.h"
#include "symbol.h"
#include "matcher.h"

class SlowLeafSequence {
private:
	const Definitions &m_definitions;
	const Expression * const m_expr;

public:
	class Element {
	private:
		const Expression * const m_expr;
		const index_t m_begin;
		BaseExpressionRef m_element;

	public:
		inline Element(const Expression *expr, index_t begin) : m_expr(expr), m_begin(begin) {
		}

		inline const BaseExpressionRef &operator*() {
			if (!m_element) {
				m_element = m_expr->materialize_leaf(m_begin);
			}
			return m_element;
		}
	};

	class Sequence {
	private:
		const Symbols &m_symbols;
		const Expression * const m_expr;
		const index_t m_begin;
		const index_t m_end;
		BaseExpressionRef m_sequence;

	public:
		inline Sequence(const Symbols &symbols, const Expression *expr, index_t begin, index_t end) :
			m_symbols(symbols), m_expr(expr), m_begin(begin), m_end(end) {
		}

		inline const BaseExpressionRef &operator*() {
			if (!m_sequence) {
				m_sequence = m_expr->slice(m_symbols.Sequence, m_begin, m_end);
			}
			return m_sequence;
		}
	};

	inline SlowLeafSequence(const Definitions &definitions, const Expression *expr) :
		m_definitions(definitions), m_expr(expr) {
	}

	inline const Definitions &definitions() const {
		return m_definitions;
	}

	inline Element element(index_t begin) const {
		return Element(m_expr, begin);
	}

	inline Sequence sequence(index_t begin, index_t end) const {
        assert(begin <= end);
		return Sequence(m_definitions.symbols(), m_expr, begin, end);
	}

	inline index_t same(index_t begin, BaseExpressionPtr other) const {
		if (other->same(m_expr->materialize_leaf(begin))) {
			return begin + 1;
		} else {
			return -1;
		}
	}
};

#define DECLARE_MATCH_EXPRESSION_METHODS                                                                      \
	virtual index_t match( 					                                                                  \
		MatchContext &context,                                                                                \
		const FastLeafSequence &sequence,																  	  \
		index_t begin,                                                                       				  \
		index_t end) const {                                                                 				  \
		return do_match<FastLeafSequence>(context, sequence, begin, end);    							  	  \
	}                                                                                                         \
																											  \
	virtual index_t match(                                                                             		  \
		MatchContext &context,                                                                                \
		const SlowLeafSequence &sequence,															  		  \
		index_t begin,                                                                                 		  \
		index_t end) const {                                                                           	      \
		return do_match<SlowLeafSequence>(context, sequence, begin, end);   			                      \
	}                                                                                                         \

#define DECLARE_MATCH_METHODS                                                                                 \
	DECLARE_MATCH_EXPRESSION_METHODS                                                                          \
	virtual index_t match(                                                                               	  \
		MatchContext &context,                                                                                \
		const CharacterSequence &sequence,																	  \
		index_t begin,                             	                                              			  \
		index_t end) const {                         	                                          			  \
        return do_match<CharacterSequence>(context, sequence, begin, end);                                    \
    }

class TerminateMatcher : public PatternMatcher {
private:
	template<typename Sequence>
	inline index_t do_match(
		MatchContext &context,
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		if (begin == end || context.anchor != MatchContext::DoAnchor) {
			return begin;
		} else {
			return -1;
		}
	}

public:
	DECLARE_MATCH_METHODS
};

class NoCondition {
public:
	template<typename Element>
	inline bool operator()(Element &item) const {
		return true;
	}
};

class HeadCondition {
private:
	const BaseExpressionRef m_head;

public:
	inline HeadCondition(const BaseExpressionRef &head) : m_head(head) {
	}

	template<typename Element>
	inline bool operator()(Element &element) const {
		const auto item = *element;
		if (item->type() != ExpressionType) {
			return false;
		} else {
			const BaseExpressionPtr a = item->as_expression()->head_ptr();
			const BaseExpressionPtr b = m_head.get();
			return a == b || a->same(b);
		}
	}

	inline bool operator()(const char item) const {
		return true;
	}
};

class Element {
private:
	const BaseExpressionRef &m_element;

public:
	inline Element(const BaseExpressionRef &element) : m_element(element) {
	}

	inline const BaseExpressionRef &operator*() const {
		return m_element;
	}
};

class NoVariable {
public:
	template<typename Element, typename F>
	inline index_t operator()(MatchContext &context, Element &element, const F &f) const {
		return f();
	}
};

class AssignVariable {
private:
	const SymbolRef m_variable;

public:
	inline AssignVariable(const SymbolRef &variable) : m_variable(variable) {
	}

	template<typename Element, typename F>
	inline index_t operator()(MatchContext &context, Element &element, const F &f) const {
		if (!m_variable->set_matched_value(context.id, *element)) {
            return -1;
        }

		try {
			const index_t match = f();

			if (match >= 0) {
				context.matched_variables.prepend(m_variable.get());
			} else {
				m_variable->clear_matched_value();
			}

			return match;
		} catch (...) {
			m_variable->clear_matched_value();
			throw;
		}
	}
};

template<typename Dummy, typename Variable>
class SameMatcher : public PatternMatcher {
private:
	const BaseExpressionRef m_patt;
	const Variable m_variable;

	template<typename Sequence>
	inline index_t do_match(
		MatchContext &context,
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		if (begin < end) {
			const index_t up_to = sequence.same(begin, m_patt.get());
			if (up_to > begin) {
				const PatternMatcherRef &next = m_next;
				auto element = Element(m_patt);
				return m_variable(context, element, [up_to, end, &next, &context, &sequence] () {
					return next->match(context, sequence, up_to, end);
				});
			} else {
				return -1;
			}
		} else {
			return -1;
		}
	}

public:
	inline SameMatcher(const BaseExpressionRef &patt, const Variable &variable) :
		m_patt(patt), m_variable(variable) {
	}

	DECLARE_MATCH_METHODS
};

template<typename Dummy, typename Variable>
class ExceptMatcher : public PatternMatcher {
private:
    const PatternMatcherRef m_matcher;
    const Variable m_variable;

    template<typename Sequence>
    inline index_t do_match(
        MatchContext &context,
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

        if (begin == end) {
			return -1;
        }

        if (m_matcher->match(context, sequence, begin, begin + 1) >= 0) {
            return -1;
        } else {
            const PatternMatcherRef &next = m_next;
			auto element = sequence.element(begin);
            return m_variable(context, element, [begin, end, &next, &context, &sequence] () {
                return next->match(context, sequence, begin + 1, end);
            });
        }
    }

public:
    inline ExceptMatcher(const PatternMatcherRef &matcher, const Variable &variable) :
        m_matcher(matcher), m_variable(variable) {
    }

    DECLARE_MATCH_METHODS
};

template<typename Dummy, typename Variable>
class AlternativesMatcher : public PatternMatcher {
private:
	std::vector<PatternMatcherRef> m_matchers;
    const Variable m_variable;

    template<typename Sequence>
    inline index_t do_match(
        MatchContext &context,
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

        for (const PatternMatcherRef &matcher : m_matchers) {
	        const index_t match = matcher->match(context, sequence, begin, end);
            if (match >= 0) {
                return match;
            }
        }

        return -1;
    }

public:
	inline AlternativesMatcher(const std::vector<PatternMatcherRef> &matchers, const Variable &variable) :
        m_matchers(matchers), m_variable(variable) {
	}

    virtual void set_next(
        const PatternMatcherRef &next) {

        for (const PatternMatcherRef &matcher : m_matchers) {
            matcher->set_next(next);
        }
    }

    virtual void set_size(
        const MatchSize &size_from_here,
        const MatchSize &size_from_next) {

        PatternMatcher::set_size(size_from_here, size_from_next);

        for (const PatternMatcherRef &matcher : m_matchers) {
            matcher->set_size(size_from_here, size_from_next);
        }
    }

    DECLARE_MATCH_METHODS
};

template<typename Condition, typename Variable>
class BlankMatcher : public PatternMatcher {
private:
	const Condition m_condition;
	const Variable m_variable;

	template<typename Sequence>
	inline index_t do_match(
		MatchContext &context,
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		auto element = sequence.element(begin);
		if (begin < end && m_condition(element)) {
			const PatternMatcherRef &next = m_next;
			return m_variable(context, element, [begin, end, &next, &context, &sequence] () {
				return next->match(context, sequence, begin + 1, end);
			});
		} else {
			return -1;
		}
	}

public:
	inline BlankMatcher(const Condition &condition, const Variable &variable) :
		m_condition(condition), m_variable(variable) {
	}

	DECLARE_MATCH_METHODS
};

template<index_t Minimum, typename Condition, typename Variable>
class GenericBlankSequenceMatcher : public PatternMatcher {
private:
	const Condition m_condition;
	const Variable m_variable;

	template<typename Sequence>
	inline index_t do_match(
		MatchContext &context,
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		const index_t n = end - begin;

		const index_t max_size = n - m_size_from_next.min();

		if (max_size < Minimum) {
			return -1;
		}

		const index_t min_size = context.anchor == MatchContext::DoAnchor ? std::max(
            n - index_t(m_size_from_next.max()), Minimum) : Minimum;

		index_t condition_max_size = max_size;
		for (size_t i = 0; i < max_size; i++) {
			auto element = sequence.element(begin + i);
			if (!m_condition(element)) {
				condition_max_size = i;
				break;
			}
		}

		const PatternMatcherRef &next = m_next;
		for (index_t i = condition_max_size; i >= min_size; i--) {
			auto part = sequence.sequence(begin, begin + i);
            const index_t match = m_variable(context, part,
                [begin, end, i, &next, &context, &sequence] () {
				    return next->match(context, sequence, begin + i, end);
			});
			if (match >= 0) {
				return match;
			}

		}

		return -1;
	}

public:
	inline GenericBlankSequenceMatcher(const Condition &condition, const Variable &variable) :
		m_condition(condition), m_variable(variable) {
	}

	DECLARE_MATCH_METHODS
};

template<typename Condition, typename Variable>
using BlankNullSequenceMatcher = GenericBlankSequenceMatcher<0, Condition, Variable>;

template<typename Condition, typename Variable>
using BlankSequenceMatcher = GenericBlankSequenceMatcher<1, Condition, Variable>;

template<typename Dummy, typename Variable>
class ExpressionMatcher : public PatternMatcher {
private:
	const PatternMatcherRef m_match_head;
	const PatternMatcherRef m_match_leaves;
	const Variable m_variable;

	template<typename Sequence>
	inline index_t do_match(
		MatchContext &context,
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		if (begin == end) {
			return -1;
		}

		const auto item = *sequence.element(begin);

		if (item->type() != ExpressionType) {
			return -1;
		}

		const Expression *expr = item->as_expression();

		const PatternMatcherRef &match_leaves = m_match_leaves;

		if (!match_leaves->might_match(expr->size())) {
			return -1;
		}

		const BaseExpressionRef &head = expr->head();

		if (m_match_head->match(context, FastLeafSequence(sequence.definitions(), &head), 0, 1) < 0) {
			return -1;
		}

		if (slice_needs_no_materialize(expr->slice_code())) {
			if (expr->with_leaves_array([&context, &sequence, &match_leaves] (const BaseExpressionRef *leaves, size_t size) {
				return match_leaves->match(context, FastLeafSequence(sequence.definitions(), leaves), 0, size);
			}) < 0) {
				return -1;
			}
		} else {
			if (match_leaves->match(context, SlowLeafSequence(sequence.definitions(), expr), 0, expr->size()) < 0) {
				return -1;
			}
		}

		const PatternMatcherRef &next = m_next;
		auto element = Element(item);
		return m_variable(context, element, [begin, end, &next, &sequence, &context] () {
			return next->match(context, sequence, begin + 1, end);
		});
	}

public:
	inline ExpressionMatcher(const std::tuple<PatternMatcherRef, PatternMatcherRef> &match, const Variable &variable) :
		m_match_head(std::get<0>(match)), m_match_leaves(std::get<1>(match)), m_variable(variable) {
	}

	DECLARE_MATCH_EXPRESSION_METHODS

	virtual index_t match(
		MatchContext &context,
		const CharacterSequence &sequence,
		index_t begin,
		index_t end) const {
		return -1;
    }
};

class PatternCompiler {
private:
	template<template<typename, typename> class Matcher, typename Parameter>
	PatternMatcherRef instantiate_matcher(
		const Parameter &parameter,
		const SymbolRef *variable);

	template<template<typename, typename> class Matcher>
	inline PatternMatcherRef create_blank_matcher(
		const BaseExpressionRef *patt_begin,
		const BaseExpressionRef *patt_end,
		const SymbolRef *variable);

	PatternMatcherRef compile_part(
		const BaseExpressionRef &patt_head,
		const BaseExpressionRef *patt_begin,
		const BaseExpressionRef *patt_end,
		const SymbolRef *variable);

public:
    PatternCompiler();

	PatternMatcherRef compile(
		const BaseExpressionRef *begin,
		const BaseExpressionRef *end,
		const SymbolRef *variable);
};

PatternCompiler::PatternCompiler() {
}

template<template<typename, typename> class Matcher, typename Parameter>
PatternMatcherRef PatternCompiler::instantiate_matcher(
	const Parameter &parameter,
	const SymbolRef *variable) {

	PatternMatcherRef matcher;

	if (variable) {
        matcher = new Matcher<Parameter, AssignVariable>(
            parameter, AssignVariable(*variable));
	} else {
		matcher = new Matcher<Parameter, NoVariable>(
			parameter, NoVariable());
	}

	return matcher;
}

PatternMatcherRef PatternCompiler::compile(
	const BaseExpressionRef *begin,
	const BaseExpressionRef *end,
	const SymbolRef *variable) {

	const size_t n = end - begin;

	std::vector<MatchSize> matchable;
	matchable.reserve(n + 1);
	MatchSize size = MatchSize::exactly(0);
	matchable.push_back(size);
	for (const BaseExpressionRef *i = end - 1; i >= begin; i--) {
		size += (*i)->match_size();
		matchable.push_back(size);
	}
	std::reverse(matchable.begin(), matchable.end());

	PatternMatcherRef initial_matcher;
	PatternMatcherRef previous_matcher;

	for (size_t i = 0; i < n; i++) {
		const BaseExpressionRef &curr = begin[i];

		PatternMatcherRef matcher;

		switch (curr->type()) {
			case ExpressionType: {
				PatternCompiler * const compiler = this;
				const Expression * const patt_expr = curr->as_expression();

				matcher = patt_expr->with_leaves_array(
					[compiler, patt_expr, variable] (const BaseExpressionRef *leaves, size_t size) {
						return compiler->compile_part(patt_expr->head(), leaves, leaves + size, variable);
					});
				break;
			}

			default:
				matcher = instantiate_matcher<SameMatcher>(curr, variable);
				break;
		}

		matcher->set_size(matchable[i], matchable[i + 1]);

		if (previous_matcher) {
			previous_matcher->set_next(matcher);
		} else {
			initial_matcher = matcher;
		}

		previous_matcher = matcher;
	}

	assert(previous_matcher);
	previous_matcher->set_next(PatternMatcherRef(new TerminateMatcher));

	return initial_matcher;
}

template<template<typename, typename> class Matcher>
inline PatternMatcherRef PatternCompiler::create_blank_matcher(
    const BaseExpressionRef *patt_begin,
    const BaseExpressionRef *patt_end,
	const SymbolRef *variable) {

    if (patt_end - patt_begin == 1 && (*patt_begin)->type() == SymbolType) {
        return instantiate_matcher<Matcher>(HeadCondition(*patt_begin), variable);
    } else {
        return instantiate_matcher<Matcher>(NoCondition(), variable);
    }
}

PatternMatcherRef PatternCompiler::compile_part(
	const BaseExpressionRef &patt_head,
	const BaseExpressionRef *patt_begin,
	const BaseExpressionRef *patt_end,
	const SymbolRef *variable) {

	switch (patt_head->extended_type()) {
		case SymbolBlank:
            return create_blank_matcher<BlankMatcher>(
                patt_begin, patt_end, variable);

	    case SymbolBlankSequence:
			return create_blank_matcher<BlankSequenceMatcher>(
				patt_begin, patt_end, variable);

		case SymbolBlankNullSequence:
			return create_blank_matcher<BlankNullSequenceMatcher>(
				patt_begin, patt_end, variable);

		case SymbolPattern:
			if (patt_end - patt_begin == 2) { // 2 leaves?
				if ((*patt_begin)->type() == SymbolType) {
					const SymbolRef variable = boost::const_pointer_cast<Symbol>(
						boost::static_pointer_cast<const Symbol>(*patt_begin));
					return compile(patt_begin + 1, patt_begin + 2, &variable);
				}
			}

		case SymbolAlternatives: {
			std::vector<PatternMatcherRef> matchers;
			matchers.reserve(patt_end - patt_begin);
			for (const BaseExpressionRef *patt = patt_begin; patt < patt_end; patt++) {
				matchers.push_back(compile(patt, patt + 1, variable));
			}

			return instantiate_matcher<AlternativesMatcher>(matchers, variable);
		}

        case SymbolExcept:
            if (patt_end - patt_begin == 1) { // 1 leaf ?
                return instantiate_matcher<ExceptMatcher>(
                    compile(patt_begin, patt_begin + 1, nullptr), variable);
            }

            // fallthrough

		default: {
			const PatternMatcherRef match_head = compile(
				&patt_head, &patt_head + 1, nullptr);

			const PatternMatcherRef match_leaves = compile(
				patt_begin, patt_end, nullptr);

			return instantiate_matcher<ExpressionMatcher>(
				std::make_tuple(match_head, match_leaves), variable);
		}
	}
}

PatternMatcherRef compile_expression_pattern(const BaseExpressionRef &patt) {
	PatternCompiler compiler;
	return compiler.compile(&patt, &patt + 1, nullptr);
}

PatternMatcherRef compile_string_pattern(const BaseExpressionRef &patt) {
	if (patt->type() == ExpressionType && patt->as_expression()->head()->extended_type() == SymbolStringExpression) {
		return patt->as_expression()->with_leaves_array([] (const BaseExpressionRef *leaves, size_t n) {
			PatternCompiler compiler;
			return compiler.compile(leaves, leaves + n, nullptr);
		});
	} else {
		PatternCompiler compiler;
		return compiler.compile(&patt, &patt + 1, nullptr);
	}
}
