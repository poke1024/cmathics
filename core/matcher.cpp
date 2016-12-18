#include "types.h"
#include "symbol.h"
#include "matcher.h"
#include "unicode/uchar.h"

class SlowLeafSequence {
private:
	MatchContext &m_context;
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
		const Evaluation &m_evaluation;
		const Expression * const m_expr;
		const index_t m_begin;
		const index_t m_end;
		BaseExpressionRef m_sequence;

	public:
		inline Sequence(const Evaluation &evaluation, const Expression *expr, index_t begin, index_t end) :
            m_evaluation(evaluation), m_expr(expr), m_begin(begin), m_end(end) {
		}

		inline const BaseExpressionRef &operator*() {
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

	inline Sequence sequence(index_t begin, index_t end) const {
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

#define DECLARE_MATCH_EXPRESSION_METHODS                                                                      \
	virtual index_t match( 					                                                                  \
		const FastLeafSequence &sequence,																  	  \
		index_t begin,                                                                       				  \
		index_t end) const {                                                                 				  \
		return do_match<FastLeafSequence>(sequence, begin, end);    	        						  	  \
	}                                                                                                         \
																											  \
	virtual index_t match(                                                                             		  \
		const SlowLeafSequence &sequence,															  		  \
		index_t begin,                                                                                 		  \
		index_t end) const {                                                                           	      \
		return do_match<SlowLeafSequence>(sequence, begin, end);   	            		                      \
	}                                                                                                         \


#define DECLARE_MATCH_CHARACTER_METHODS																		  \
	virtual index_t match(                                                                               	  \
		const AsciiCharacterSequence &sequence,	    														  \
		index_t begin,                             	                                              			  \
		index_t end) const {                         	                                          			  \
        return do_match<AsciiCharacterSequence>(sequence, begin, end);                                        \
    }                                                                                                         \
                                                                                                              \
	virtual index_t match(                                                                               	  \
		const SimpleCharacterSequence &sequence,												    		  \
		index_t begin,                             	                                              			  \
		index_t end) const {                         	                                          			  \
        return do_match<SimpleCharacterSequence>(sequence, begin, end);                                       \
    }                                                                                                         \
                                                                                                              \
	virtual index_t match(                                                                               	  \
		const ComplexCharacterSequence &sequence,												    		  \
		index_t begin,                             	                                              			  \
		index_t end) const {                         	                                          			  \
        return do_match<ComplexCharacterSequence>(sequence, begin, end);                                      \
    }

#define DECLARE_NO_MATCH_CHARACTER_METHODS                                                                    \
	virtual index_t match(                                                                               	  \
		const AsciiCharacterSequence &sequence,	    														  \
		index_t begin,                             	                                              			  \
		index_t end) const {                         	                                          			  \
        return -1;                                                                                            \
    }                                                                                                         \
                                                                                                              \
	virtual index_t match(                                                                               	  \
		const SimpleCharacterSequence &sequence,												    		  \
		index_t begin,                             	                                              			  \
		index_t end) const {                         	                                          			  \
        return -1;                                                                                            \
    }                                                                                                         \
                                                                                                              \
	virtual index_t match(                                                                               	  \
		const ComplexCharacterSequence &sequence,												    		  \
		index_t begin,                             	                                              			  \
		index_t end) const {                         	                                          			  \
        return -1;                                                                                            \
    }

#define DECLARE_MATCH_METHODS                                                                                 \
	DECLARE_MATCH_EXPRESSION_METHODS                                                                          \
	DECLARE_MATCH_CHARACTER_METHODS

class StartMatcher : public PatternMatcher {
private:
	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		if (begin != 0) {
			return -1;
		} else if (end == 0) {
			return 0;
		} else {
			return m_next->match(sequence, begin, end);
		}
	}

public:
	DECLARE_MATCH_METHODS
};

class EndMatcher : public PatternMatcher {
private:
	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		if (begin == end) {
			return begin;
		} else {
			return -1;
		}
	}

public:
	DECLARE_MATCH_METHODS
};

class TerminateMatcher : public PatternMatcher {
private:
	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		if (begin == end || (sequence.context().options & MatchContext::NoEndAnchor)) {
			return begin;
		} else {
			return -1;
		}
	}

public:
	DECLARE_MATCH_METHODS
};

class NoTest {
public:
	static constexpr bool is_noop = true;

	template<typename Element>
	inline bool operator()(Element &item, const Evaluation &evaluation) const {
		return true;
	}
};

class HeadTest {
private:
	const BaseExpressionRef m_head;

public:
	static constexpr bool is_noop = false;

	inline HeadTest(const BaseExpressionRef &head) : m_head(head) {
	}

	template<typename Element>
	inline bool operator()(Element &element, const Evaluation &evaluation) const {
		const auto item = *element;
        const BaseExpressionPtr a = item->head(evaluation);
        const BaseExpressionPtr b = m_head.get();
        return a == b || a->same(b);
	}
};

class PatternTest {
private:
    const BaseExpressionRef m_test;

public:
    inline PatternTest(const BaseExpressionRef &test) :
        m_test(test) {
    }

    template<typename Element>
    inline bool operator()(Element &element, const Evaluation &evaluation) const {
        return expression(m_test, *element)->evaluate(evaluation)->is_true();
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

template<typename Match, typename Variable>
class ElementMatcher : public PatternMatcher {
private:
	const Match m_match;
	const Variable m_variable;

	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		if (begin < end) {
			auto result = m_match(sequence, begin, end);
            const index_t up_to = std::get<1>(result);
			if (up_to > begin) {
				const PatternMatcherRef &next = m_next;
				return m_variable(sequence.context(), std::get<0>(result), [up_to, end, &next, &sequence] () {
					return next->match(sequence, up_to, end);
				});
			} else {
				return -1;
			}
		} else {
			return -1;
		}
	}

public:
    DECLARE_MATCH_METHODS

	inline ElementMatcher(const Match &match, const Variable &variable) :
		m_match(match), m_variable(variable) {
	}
};

template<typename Match>
class PositionMatcher : public PatternMatcher {
private:
	const Match m_match;

	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		if (m_match(sequence, begin, end)) {
			return m_next->match(sequence, begin, end);
		} else {
			return -1;
		}
	}

public:
	DECLARE_MATCH_METHODS

	inline PositionMatcher(const Match &match) :
		m_match(match) {
	}
};

class MatchSame {
private:
    const BaseExpressionRef m_patt;

public:
    inline MatchSame(const BaseExpressionRef &patt) : m_patt(patt) {
    }

    template<typename Sequence>
    inline auto operator()(const Sequence &sequence, index_t begin, index_t end) const {
        return std::make_tuple(Element(m_patt), sequence.same(begin, m_patt.get()));
    }
};


template<typename Test>
class MatchCharacterBlank {
private:
	const Test m_test;

public:
	inline MatchCharacterBlank(const Test &test) : m_test(test) {
	}

	template<typename Sequence>
	inline auto operator()(const Sequence &sequence, index_t begin, index_t end) const {
		const auto &test = m_test;

		const auto test_code_point = [&test] (auto p) {
			return test(p);
		};

		if (sequence.all_code_points(begin, test_code_point)) {
			return std::make_tuple(sequence.element(begin), begin + 1);
		} else {
			return std::make_tuple(sequence.element(begin), index_t(-1));
		}
	}
};

template<typename Test>
class MatchCharacterBlankSequence {
private:
	const Test m_test;

public:
	inline MatchCharacterBlankSequence(const Test &test) : m_test(test) {
	}

	template<typename Sequence>
	inline auto operator()(const Sequence &sequence, index_t begin, index_t end) const {
		const auto &test = m_test;

		const auto test_code_point = [&test] (auto p) {
			return test(p);
		};

		index_t up_to = begin;
		do {
			if (sequence.all_code_points(up_to, test_code_point)) {
				break;
			}
			up_to++;
		} while (up_to < end);

		if (up_to > begin) {
			return std::make_tuple(sequence.sequence(begin, up_to), up_to);
		} else {
			return std::make_tuple(sequence.sequence(begin, up_to), index_t(-1));
		}
	}
};

inline bool is_newline(uint32_t code) {
	// see http://userguide.icu-project.org/strings/regexp
	switch (code) {
		case 0x000a:
		case 0x000b:
		case 0x000c:
		case 0x000d:
		case 0x0085:
		case 0x2028:
		case 0x2029:
			return true;
	}
	return false;
}

class StartOfLine {
public:
	template<typename Sequence>
	inline bool operator()(const Sequence &sequence, index_t begin, index_t end) const {
		return begin == 0 || sequence.all_code_points(begin - 1, [] (auto p) {
			return is_newline(p);
		});
	}
};

class EndOfLine {
public:
	template<typename Sequence>
	inline bool operator()(const Sequence &sequence, index_t begin, index_t end) const {
		return begin >= end - 1 || sequence.all_code_points(begin + 1, [] (auto p) {
			return is_newline(p);
		});
	}
};

class WordBoundary {
public:
	template<typename Sequence>
	inline bool operator()(const Sequence &sequence, index_t begin, index_t end) const {
		return sequence.is_word_boundary(begin);
	}
};

template<typename MatchAsCharacter, typename MatchAsExpression>
class MatchCharacterOrExpression {
private:
	const MatchAsCharacter m_match_character;
	const MatchAsExpression m_match_expression;

public:
	inline MatchCharacterOrExpression(
		const MatchAsCharacter &match_character,
        const MatchAsExpression &match_expression) :
		m_match_character(match_character),
		m_match_expression(match_expression) {
	}

	inline auto operator()(const SlowLeafSequence &sequence, index_t begin, index_t end) const {
		return m_match_expression(sequence, begin, end);
	}

	inline auto operator()(const FastLeafSequence &sequence, index_t begin, index_t end) const {
		return m_match_expression(sequence, begin, end);
	}

	inline auto operator()(const AsciiCharacterSequence &sequence, index_t begin, index_t end) const {
		return m_match_character(sequence, begin, end);
	}

	inline auto operator()(const SimpleCharacterSequence &sequence, index_t begin, index_t end) const {
		return m_match_character(sequence, begin, end);
	}

	inline auto operator()(const ComplexCharacterSequence &sequence, index_t begin, index_t end) const {
		return m_match_character(sequence, begin, end);
	}
};

class MatchToFalse {
public:
	template<typename Sequence>
	inline bool operator()(const Sequence &sequence, index_t begin, index_t end) const {
		return false;
	}
};

template<typename Match>
using MatchCharacterPosition = MatchCharacterOrExpression<Match, MatchToFalse>;

class MatchToPattern {
private:
	const BaseExpressionRef m_patt;

public:
	inline MatchToPattern(const BaseExpressionRef &patt) : m_patt(patt) {
	}

	template<typename Sequence>
	inline auto operator()(const Sequence &sequence, index_t begin, index_t end) const {
		return std::make_tuple(Element(m_patt), sequence.same(begin, m_patt.get()));
	}
};

template<typename Match>
using MatchCharacter = MatchCharacterOrExpression<Match, MatchToPattern>;

template<typename Dummy, typename Variable>
class SameMatcher : public ElementMatcher<MatchSame, Variable> {
public:
    inline SameMatcher(const BaseExpressionRef &patt, const Variable &variable) :
	    ElementMatcher<MatchSame, Variable>(MatchSame(patt), variable) {
    }

    DECLARE_NO_MATCH_CHARACTER_METHODS
};

template<typename Dummy, typename Variable>
class SameStringMatcher : public ElementMatcher<MatchSame, Variable> {
public:
    inline SameStringMatcher(const BaseExpressionRef &patt, const Variable &variable) :
	    ElementMatcher<MatchSame, Variable>(MatchSame(patt), variable) {
    }
};

template<typename Dummy, typename Variable>
class ExceptMatcher : public PatternMatcher {
private:
    const PatternMatcherRef m_matcher;
    const Variable m_variable;

    template<typename Sequence>
    inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

        if (begin == end) {
			return -1;
        }

        if (m_matcher->match(sequence, begin, begin + 1) >= 0) {
            return -1;
        } else {
            const PatternMatcherRef &next = m_next;
			auto element = sequence.element(begin);
            return m_variable(sequence.context(), element, [begin, end, &next, &sequence] () {
                return next->match(sequence, begin + 1, end);
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
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

        for (const PatternMatcherRef &matcher : m_matchers) {
	        const index_t match = matcher->match(sequence, begin, end);
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

template<typename Tests, typename Variable>
class BlankMatcher : public PatternMatcher {
private:
	const Tests m_tests;
	const Variable m_variable;

	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		auto element = sequence.element(begin);

		const auto &test_head = std::get<0>(m_tests);
		const auto &test_patt = std::get<1>(m_tests);

		if (begin < end &&
			test_head(element, sequence.context().evaluation) &&
			test_patt(element, sequence.context().evaluation)) {
			const PatternMatcherRef &next = m_next;
			return m_variable(sequence.context(), element, [begin, end, &next, &sequence] () {
				return next->match(sequence, begin + 1, end);
			});
		} else {
			return -1;
		}
	}

public:
	inline BlankMatcher(
        const Tests &tests,
        const Variable &variable) :

		m_tests(tests),
        m_variable(variable) {
	}

	DECLARE_MATCH_METHODS
};

template<typename Tests>
class Longest {
private:
	const Tests m_tests;

public:
	template<typename Sequence, typename F>
	inline index_t operator()(
		const Sequence &sequence,
		const index_t begin,
		const index_t min_size,
		const index_t max_size,
		const F &f) const {

		const auto &test_head = std::get<0>(m_tests);
		index_t condition_max_size = max_size;
		if (!test_head.is_noop) {
			for (size_t i = 0; i < max_size; i++) {
				auto element = sequence.element(begin + i);
				if (!test_head(element, sequence.context().evaluation)) {
					condition_max_size = i;
					break;
				}
			}
		}

		const auto &test_patt = std::get<1>(m_tests);
		for (index_t i = condition_max_size; i >= min_size; i--) {
			const index_t match = f(test_patt, i);
			if (match >= 0) {
				return match;
			}
		}

		return -1;
	}

	inline Longest(const Tests &tests) : m_tests(tests) {
	}
};

template<typename Tests>
class Shortest {
private:
	const Tests m_tests;

public:
	template<typename Sequence, typename F>
	inline index_t operator()(
		const Sequence &sequence,
		const index_t begin,
		const index_t min_size,
		const index_t max_size,
		const F &f) const {

		const auto &test_head = std::get<0>(m_tests);
		if (!test_head.is_noop) {
			for (size_t i = 0; i < min_size; i++) {
				auto element = sequence.element(begin + i);
				if (!test_head(element, sequence.context().evaluation)) {
					return -1;
				}
			}
		}

		const auto &test_patt = std::get<1>(m_tests);
		for (index_t i = min_size; i <= max_size; i++) {
			if (!test_head.is_noop) {
				auto element = sequence.element(begin + i);
				if (!test_head(element, sequence.context().evaluation)) {
					return -1;
				}
			}
			const index_t match = f(test_patt, i);
			if (match >= 0) {
				return match;
			}
		}

		return -1;
	}

	inline Shortest(const Tests &tests) : m_tests(tests) {
	}
};

template<template<typename> class Strategy, index_t Minimum, typename Tests, typename Variable>
class BlankSequenceMatcher : public PatternMatcher {
private:
	const Strategy<Tests> m_strategy;
	const Variable m_variable;

	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		const index_t n = end - begin;

		const index_t max_size = n - m_size_from_next.min();

		if (max_size < Minimum) {
			return -1;
		}

		const index_t min_size = (sequence.context().options & MatchContext::NoEndAnchor) ?
			Minimum : std::max(n - index_t(m_size_from_next.max()), Minimum);

		const auto &next = m_next;
		const auto &variable = m_variable;

		return m_strategy(
			sequence,
			begin,
			min_size,
			max_size,
			[begin, end, &sequence, &variable, &next] (const auto &test_patt, index_t i) {
				auto part = sequence.sequence(begin, begin + i);
				if (test_patt(part, sequence.context().evaluation)) {
					return variable(
						sequence.context(),
						part,
						[begin, end, i, &next, &sequence] () {
							return next->match(sequence, begin + i, end);
						});
				} else {
					return index_t(-1);
				}
			});
	}

public:
	inline BlankSequenceMatcher(
		const Tests &tests,
        const Variable &variable) :

		m_strategy(tests),
        m_variable(variable) {
	}

	DECLARE_MATCH_METHODS
};

template<typename Dummy, typename Variable>
class ExpressionMatcher : public PatternMatcher {
private:
	const PatternMatcherRef m_match_head;
	const PatternMatcherRef m_match_leaves;
	const Variable m_variable;

	template<typename Sequence>
	inline index_t do_match(
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

		if (m_match_head->match(FastLeafSequence(sequence.context(), &head), 0, 1) < 0) {
			return -1;
		}

		if (slice_needs_no_materialize(expr->slice_code())) {
			if (expr->with_leaves_array([&sequence, &match_leaves] (const BaseExpressionRef *leaves, size_t size) {
				return match_leaves->match(FastLeafSequence(sequence.context(), leaves), 0, size);
			}) < 0) {
				return -1;
			}
		} else {
			if (match_leaves->match(SlowLeafSequence(sequence.context(), expr), 0, expr->size()) < 0) {
				return -1;
			}
		}

		const PatternMatcherRef &next = m_next;
		auto element = Element(item);
		return m_variable(sequence.context(), element, [begin, end, &next, &sequence] () {
			return next->match(sequence, begin + 1, end);
		});
	}

public:
	inline ExpressionMatcher(const std::tuple<PatternMatcherRef, PatternMatcherRef> &match, const Variable &variable) :
		m_match_head(std::get<0>(match)), m_match_leaves(std::get<1>(match)), m_variable(variable) {
	}

	DECLARE_MATCH_EXPRESSION_METHODS
    DECLARE_NO_MATCH_CHARACTER_METHODS
};

class PatternCompiler {
private:
	PatternMatcherRef character_intrinsic_matcher(
		const BaseExpressionRef &curr,
		const SymbolRef *variable) const;

	PatternMatcherRef compile_element(
		const BaseExpressionRef &curr,
		const SymbolRef *variable,
		const BaseExpressionRef *test,
		const bool shortest);

	PatternMatcherRef compile_sequence(
		const BaseExpressionRef &patt_head,
		const BaseExpressionRef *patt_begin,
		const BaseExpressionRef *patt_end,
		const SymbolRef *variable,
        const BaseExpressionRef *test,
		const bool shortest);

public:
    PatternCompiler();

	PatternMatcherRef compile(
		const BaseExpressionRef *begin,
		const BaseExpressionRef *end,
		const SymbolRef *variable,
        const BaseExpressionRef *test,
		const bool shortest);
};

PatternCompiler::PatternCompiler() {
}

template<template<typename, typename> class Matcher, typename Parameter>
PatternMatcherRef instantiate_matcher(
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

PatternMatcherRef PatternCompiler::character_intrinsic_matcher(
	const BaseExpressionRef &curr,
	const SymbolRef *variable) const {

	const PatternCompiler * const self = this;

	auto create_blank = [self, &curr, &variable] (const auto &test) {
		const auto match_class = MatchCharacter<MatchCharacterBlank<decltype(test)>>(
			MatchCharacterBlank<decltype(test)>(test), MatchToPattern(curr));
		return instantiate_matcher<ElementMatcher>(match_class, variable);
	};

	auto create_blank_sequence = [self, &curr, &variable] (const auto &test) {
		const auto match_class = MatchCharacter<MatchCharacterBlankSequence<decltype(test)>>(
			MatchCharacterBlankSequence<decltype(test)>(test), MatchToPattern(curr));
		return instantiate_matcher<ElementMatcher>(match_class, variable);
	};

	switch (curr->extended_type()) {
		case SymbolStartOfString:
			return new StartMatcher();

		case SymbolEndOfString:
			return new EndMatcher();

		case SymbolStartOfLine:
			return new PositionMatcher<MatchCharacterPosition<StartOfLine>>(
				MatchCharacterPosition<StartOfLine>(StartOfLine(), MatchToFalse()));

		case SymbolEndOfLine:
			return new PositionMatcher<MatchCharacterPosition<EndOfLine>>(
				MatchCharacterPosition<EndOfLine>(EndOfLine(), MatchToFalse()));

		case SymbolWordBoundary:
			return new PositionMatcher<MatchCharacterPosition<WordBoundary>>(
				MatchCharacterPosition<WordBoundary>(WordBoundary(), MatchToFalse()));

		case SymbolDigitCharacter:
			return create_blank([] (auto p) {
				return u_isdigit(p);
			});

		case SymbolWhitespace:
			return create_blank_sequence([] (auto p) {
				return u_isWhitespace(p);
			});

		case SymbolWhitespaceCharacter:
			return create_blank([] (auto p) {
				return u_isWhitespace(p);
			});

		case SymbolWordCharacter:
			return create_blank([] (auto p) {
				return u_isalnum(p);
			});

		case SymbolLetterCharacter:
			return create_blank([] (auto p) {
				return u_isalpha(p);
			});

		case SymbolHexidecimalCharacter:
			return create_blank([] (auto p) {
				return u_isxdigit(p);
			});

		default:
			return PatternMatcherRef();
	}
}

PatternMatcherRef PatternCompiler::compile_element(
	const BaseExpressionRef &curr,
	const SymbolRef *variable,
	const BaseExpressionRef *test,
	const bool shortest) {

	PatternMatcherRef matcher;

	switch (curr->type()) {
		case ExpressionType: {
			PatternCompiler * const compiler = this;
			const Expression * const patt_expr = curr->as_expression();

			matcher = patt_expr->with_leaves_array(
				[compiler, patt_expr, variable, test, shortest]
				(const BaseExpressionRef *leaves, size_t size) {
					return compiler->compile_sequence(
						patt_expr->head(), leaves, leaves + size, variable, test, shortest);
				});
			break;
		}

		case StringType:
			assert(test == nullptr); // FIXME
			matcher = instantiate_matcher<SameStringMatcher>(curr, variable);
			break;

		case SymbolType: {
			matcher = character_intrinsic_matcher(curr, variable);
			if (matcher) {
				break;
			}

			// fallthrough to generic SameMatcher
		}

		default:
			assert(test == nullptr); // FIXME
			matcher = instantiate_matcher<SameMatcher>(curr, variable);
			break;
	}

	return matcher;
}

PatternMatcherRef PatternCompiler::compile(
	const BaseExpressionRef *begin,
	const BaseExpressionRef *end,
	const SymbolRef *variable,
    const BaseExpressionRef *test,
	const bool shortest) {

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
		const PatternMatcherRef matcher = compile_element(
			begin[i], variable, test, shortest);

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
inline PatternMatcherRef create_blank_matcher(
    const BaseExpressionRef *patt_begin,
    const BaseExpressionRef *patt_end,
	const SymbolRef *variable,
    const BaseExpressionRef *test) {

    const bool has_head = patt_end - patt_begin == 1 && (*patt_begin)->type() == SymbolType;

    if (test) {
        if (has_head) {
            return instantiate_matcher<Matcher>(
                std::make_tuple(HeadTest(*patt_begin), PatternTest(*test)),
                variable);
        } else {
            return instantiate_matcher<Matcher>(
                std::make_tuple(NoTest(), PatternTest(*test)),
                variable);
        }
    } else {
        if (has_head) {
            return instantiate_matcher<Matcher>(
                std::make_tuple(HeadTest(*patt_begin), NoTest()),
                variable);
        } else {
            return instantiate_matcher<Matcher>(
				std::make_tuple(NoTest(), NoTest()),
                variable);
        }
    }
}

template<template<template<typename> class, index_t, typename, typename> class Matcher, index_t Minimum>
class create_blank_sequence_matcher {
private:
	template<typename Tests, typename Variable>
	using MatchLongest = Matcher<Longest, Minimum, Tests, Variable>;

	template<typename Tests, typename Variable>
	using MatchShortest = Matcher<Shortest, Minimum, Tests, Variable>;

public:
	inline PatternMatcherRef operator()(
		const BaseExpressionRef *patt_begin,
		const BaseExpressionRef *patt_end,
		const SymbolRef *variable,
		const BaseExpressionRef *test,
		const bool shortest) const {
		if (shortest) {
			return create_blank_matcher<MatchShortest>(patt_begin, patt_end, variable, test);
		} else {
			return create_blank_matcher<MatchLongest>(patt_begin, patt_end, variable, test);
		}
	}
};

PatternMatcherRef PatternCompiler::compile_sequence(
	const BaseExpressionRef &patt_head,
	const BaseExpressionRef *patt_begin,
	const BaseExpressionRef *patt_end,
	const SymbolRef *variable,
    const BaseExpressionRef *test,
	const bool shortest) {

	switch (patt_head->extended_type()) {
		case SymbolBlank:
            return create_blank_matcher<BlankMatcher>(
                patt_begin, patt_end, variable, test);

	    case SymbolBlankSequence:
			return create_blank_sequence_matcher<BlankSequenceMatcher, 1>()(
				patt_begin, patt_end, variable, test, shortest);

		case SymbolBlankNullSequence:
			return create_blank_sequence_matcher<BlankSequenceMatcher, 0>()(
				patt_begin, patt_end, variable, test, shortest);

		case SymbolAlternatives: {
			std::vector<PatternMatcherRef> matchers;
			matchers.reserve(patt_end - patt_begin);
			for (const BaseExpressionRef *patt = patt_begin; patt < patt_end; patt++) {
				matchers.push_back(compile(patt, patt + 1, variable, test, shortest));
			}
			return instantiate_matcher<AlternativesMatcher>(matchers, variable);
		}

        case SymbolExcept:
            if (patt_end - patt_begin == 1) { // 1 leaf ?
                return instantiate_matcher<ExceptMatcher>(
                    compile(patt_begin, patt_begin + 1, nullptr, test, shortest), variable);
            }
			break;

		case SymbolShortest:
			if (patt_end - patt_begin == 1) { // 1 leaf ?
				return compile_element(*patt_begin, variable, test, true);
			}
			break;

		case SymbolLongest:
			if (patt_end - patt_begin == 1) { // 1 leaf ?
				return compile_element(*patt_begin, variable, test, false);
			}
			break;

		case SymbolPattern:
			if (patt_end - patt_begin == 2) { // 2 leaves?
				if ((*patt_begin)->type() == SymbolType) {
					const SymbolRef variable = boost::const_pointer_cast<Symbol>(
						boost::static_pointer_cast<const Symbol>(patt_begin[0]));
					return compile(patt_begin + 1, patt_begin + 2, &variable, test, shortest);
				}
			}
			break;

		case SymbolPatternTest:
			if (patt_end - patt_begin == 2) { // 2 leaves?
                const BaseExpressionRef test = patt_begin[1];
                return compile(patt_begin, patt_begin + 1, variable, &test, shortest);
			}
			break;

		default:
			break;
	}

	const PatternMatcherRef match_head = compile(
		&patt_head, &patt_head + 1, nullptr, nullptr, shortest);

	const PatternMatcherRef match_leaves = compile(
		patt_begin, patt_end, nullptr, nullptr, shortest);

    assert(test == nullptr); // FIXME

	return instantiate_matcher<ExpressionMatcher>(
		std::make_tuple(match_head, match_leaves), variable);
}

PatternMatcherRef compile_expression_pattern(const BaseExpressionRef &patt) {
	PatternCompiler compiler;
	return compiler.compile(&patt, &patt + 1, nullptr, nullptr, false);
}

PatternMatcherRef compile_string_pattern(const BaseExpressionRef &patt) {
	if (patt->type() == ExpressionType && patt->as_expression()->head()->extended_type() == SymbolStringExpression) {
		return patt->as_expression()->with_leaves_array([] (const BaseExpressionRef *leaves, size_t n) {
			PatternCompiler compiler;
			return compiler.compile(leaves, leaves + n, nullptr, nullptr, false);
		});
	} else {
		PatternCompiler compiler;
		return compiler.compile(&patt, &patt + 1, nullptr, nullptr, false);
	}
}

index_t PatternMatcher::match(
    MatchContext &context,
    const String *string,
    index_t begin,
    index_t end) const {

    switch (string->extent_type()) {
        case StringExtent::ascii: {
            const AsciiCharacterSequence sequence(context, string);
            return match(sequence, begin, end);
        }
        case StringExtent::simple: {
            const SimpleCharacterSequence sequence(context, string);
            return match(sequence, begin, end);
        }
        case StringExtent::complex: {
            const ComplexCharacterSequence sequence(context, string);
            return match(sequence, begin, end);
        }
        default: {
            throw std::runtime_error("unsupported string extent type");
        }
    }
}
