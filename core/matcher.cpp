#include "types.h"
#include "symbol.h"
#include "matcher.h"
#include "unicode/uchar.h"

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

template<typename Dummy, typename MatchRest>
class ExpressionMatcher : public PatternMatcher {
private:
    const HeadLeavesMatcher m_matcher;
    const MatchRest m_rest;

    template<typename Sequence>
    inline index_t do_match(
        const Sequence &sequence,
        index_t begin,
        index_t end) const {

        if (begin == end) {
            return -1;
        }

        const auto item = *sequence.element(begin);

        if (!item->is_expression()) {
            return -1;
        }

        if (!m_matcher.with_head(sequence.context(), item->as_expression())) {
            return -1;
        }

        auto element = Element(item);
        return m_rest(sequence, begin + 1, end, element);
    }

public:
    inline ExpressionMatcher(
        const std::tuple<PatternMatcherRef, PatternMatcherRef> &match,
        const MatchRest &next) :

        m_matcher(std::get<0>(match), std::get<1>(match)),
        m_rest(next) {
    }

    virtual std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "ExpressionMatcher(" << m_matcher.name(context) << "), ";
        s << m_rest.name(context);
        return s.str();
    }


    virtual const HeadLeavesMatcher *head_leaves_matcher() const {
        return &m_matcher;
    }

    DECLARE_MATCH_EXPRESSION_METHODS
    DECLARE_NO_MATCH_CHARACTER_METHODS
};

template<typename Dummy, typename MatchRest>
class StartMatcher : public PatternMatcher {
private:
	const MatchRest &m_rest;

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
			return m_rest(sequence, begin, end);
		}
	}

public:
	inline StartMatcher(const Dummy &dummy, const MatchRest &rest) : m_rest(rest) {
	}

	DECLARE_MATCH_METHODS

    virtual std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "StartMatcher, ";
        s << m_rest.name(context);
        return s.str();
    }
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
    virtual std::string name(const MatchContext &context) const {
        return "EndMatcher";
    }

	DECLARE_MATCH_METHODS
};

class TestNone {
public:
    inline std::string name(const MatchContext &context) const {
        return "TestNone()";
    }

	template<typename Sequence, typename Element>
	inline index_t operator()(const Sequence &sequence, Element &element) const {
		return element.begin() + 1;
	};

	template<typename Strategy, typename... Args>
	inline index_t greedy(const Strategy &strategy, const Args&... args) const {
		return strategy.no_test(args...);
	}
};

class SameSymbolHead {
public:
	inline bool operator()(BaseExpressionPtr a, BaseExpressionPtr b) const {
		return a == b;
	}
};

class SameGenericHead {
public:
	inline bool operator()(BaseExpressionPtr a, BaseExpressionPtr b) const {
		return a == b || a->same(b);
	}
};

template<typename Same>
class TestHead {
private:
	const BaseExpressionRef m_head;
	const Same m_same;

public:
	inline TestHead(const BaseExpressionRef &head) : m_head(head), m_same() {
	}

    inline std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "TestHead(" << m_head->debug(context.evaluation) << ")";
        return s.str();
    }

	template<typename Sequence, typename Element>
	inline index_t operator()(const Sequence &sequence, Element &element) const {
		const auto item = *element;
		if (m_same(m_head.get(), item->head(sequence.context().evaluation))) {
			return element.begin() + 1;
		} else {
			return -1;
		}
	}

	template<typename Strategy, typename... Args>
	inline index_t greedy(const Strategy &strategy, const Args&... args) const {
		const TestHead &self = *this;
		return strategy.simple_test(
			[&self] (const auto &sequence, index_t begin, index_t end) {
				auto element = sequence.element(begin);
				return self(sequence, element);
			},
			1,
			args...);
	}
};

class SimpleGreedy {
private:
	const index_t m_fixed_size;

public:
	inline SimpleGreedy(const index_t fixed_size) : m_fixed_size(fixed_size) {
	}

	template<typename Strategy, typename ElementTest, typename... Args>
	inline index_t operator()(const Strategy &strategy, const ElementTest &element_test, Args... args) const {
		return strategy.simple_test(element_test, m_fixed_size, args...);
	}
};

class ComplexGreedy {
public:
	template<typename Strategy, typename... Args>
	inline index_t operator()(const Strategy &strategy, Args... args) const {
		return strategy.complex_test(args...);
	}
};

template<typename Callback>
class TestRepeated {
private:
	const PatternMatcherRef m_patt;
	const Callback m_callback;

public:
	inline TestRepeated(const PatternMatcherRef &patt, const Callback& callback) :
		m_patt(patt), m_callback(callback) {
	}

	template<typename Strategy, typename... Args>
	inline index_t greedy(const Strategy &strategy, const Args&... args) const {
		const PatternMatcherRef &patt = m_patt;
		return m_callback(
			strategy,
			[&patt] (const auto &sequence, index_t begin, index_t end) {
				return patt->match(sequence, begin, end);
			},
			args...);
	}
};

class PatternTest {
private:
    const BaseExpressionRef m_test;

public:
    inline PatternTest(const BaseExpressionRef &test) :
        m_test(test) {
    }

	template<typename Sequence, typename Slice>
	inline bool operator()(const Sequence &sequence, Slice &slice) const {
        return expression(m_test, *slice)->evaluate(sequence.context().evaluation)->is_true();
	}
};

class NoPatternTest {
public:
	template<typename Sequence, typename Slice>
	inline bool operator()(const Sequence &sequence, Slice &slice) const {
		return true;
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
	const index_t m_slot_index; // refers to related CompiledVariables

public:
	inline AssignVariable(const index_t slot_index) : m_slot_index(slot_index) {
	}

	template<typename Element, typename F>
	inline index_t operator()(MatchContext &context, Element &element, const F &f) const {
        bool is_owner;

		if (!context.match->assign(m_slot_index, *element, is_owner)) {
			return -1;
		}

		const index_t match = f();

		if (match < 0 && is_owner) {
			context.match->unassign(m_slot_index);
		}

		return match;
	}
};

class Continue {
private:
    const PatternMatcherRef m_next;

public:
    inline Continue(const PatternMatcherRef &next) : m_next(next) {
    }

    template<typename Sequence>
    inline index_t operator()(const Sequence &sequence, index_t begin, index_t end) const {
        return m_next->match(sequence, begin, end);
    }

    inline std::string name(const MatchContext &context) const {
        return m_next->name(context);
    }
};

class Terminate {
public:
    template<typename Sequence>
    inline index_t operator()(const Sequence &sequence, index_t begin, index_t end) const {
        if (begin == end || (sequence.context().options & MatchContext::NoEndAnchor)) {
            return begin;
        } else {
            return -1;
        }
    }

    inline std::string name(const MatchContext &context) const {
        return "Terminate";
    }
};

class Unanchored {
public:
    template<typename Sequence>
    inline index_t operator()(const Sequence &sequence, index_t begin, index_t end) const {
        return begin;
    }

    inline std::string name(const MatchContext &context) const {
        return "Unanchored";
    }
};

template<typename PatternTest, typename Variable, typename Continue>
class RestMatcher {
private:
    const PatternTest m_pattern_test; // originates from PatternTest[]
    const Variable m_variable;
    const Continue m_continue;

public:
    template<typename Sequence>
    inline index_t operator()(const Sequence &sequence, index_t begin, index_t end) const {
        return m_continue(sequence, begin, end);
    }

    template<typename Sequence, typename Matched>
    inline index_t operator()(const Sequence &sequence, index_t begin, index_t end, Matched &matched) const {
        if (m_pattern_test(sequence, matched)) {
            const Continue &cont = m_continue;
            return m_variable(sequence.context(), matched, [&cont, &sequence, begin, end] () {
                return cont(sequence, begin, end);
            });
        } else {
            return -1;
        }
    }

    inline RestMatcher(const PatternTest &test, const Variable &variable, const Continue &cont) :
        m_pattern_test(test), m_variable(variable), m_continue(cont) {
    }

    inline std::string name(const MatchContext &context) const {
        return m_continue.name(context);
    }
};

template<typename MatchRest>
class EmptyMatcher : public PatternMatcher {
private:
	const MatchRest m_rest;

	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		return m_rest(sequence, begin, end);
	}

public:
	DECLARE_MATCH_METHODS

	inline EmptyMatcher(const MatchRest &next) :
		m_rest(next) {
	}

	virtual std::string name(const MatchContext &context) const {
		std::ostringstream s;
		s << "EmptyMatcher(), ";
		s << m_rest.name(context);
		return s.str();
	}
};

template<typename Match, typename MatchRest>
class ElementMatcher : public PatternMatcher {
private:
	const Match m_match;
    const MatchRest m_rest;

	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		if (begin < end) {
			auto result = m_match(sequence, begin, end);
            const index_t up_to = std::get<1>(result);
			if (up_to > begin) {
                return m_rest(sequence, up_to, end, std::get<0>(result));
			} else {
				return -1;
			}
		} else {
			return -1;
		}
	}

public:
    DECLARE_MATCH_METHODS

	inline ElementMatcher(const Match &match, const MatchRest &next) :
		m_match(match), m_rest(next) {
	}

    virtual std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "ElementMatcher(" << m_match.name(context) << "), ";
        s << m_rest.name(context);
        return s.str();
    }
};

template<typename Match, typename MatchRest>
class PositionMatcher : public PatternMatcher {
private:
	const Match m_match;
    const MatchRest m_rest;

	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		if (m_match(sequence, begin, end)) {
			return m_rest(sequence, begin, end);
		} else {
			return -1;
		}
	}

public:
	DECLARE_MATCH_METHODS

	inline PositionMatcher(const Match &match, const MatchRest &next) :
		m_match(match), m_rest(next) {
	}

    virtual std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "PositionMatcher(" << m_match.name(context) << "), ";
        s << m_rest.name(context);
        return s.str();
    }
};

class MatchSame {
private:
    const BaseExpressionRef m_patt;

public:
    inline MatchSame(const BaseExpressionRef &patt) : m_patt(patt) {
    }

    inline std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "MatchSame(" << m_patt->debug(context.evaluation) << ")";
        return s.str();
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

    inline std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "MatchCharacterBlank(), ";
        return s.str();
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
			return std::make_tuple(sequence.slice(begin, up_to), up_to);
		} else {
			return std::make_tuple(sequence.slice(begin, up_to), index_t(-1));
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

    inline std::string name(const MatchContext &context) const {
        return "StartOfLine";
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

    inline std::string name(const MatchContext &context) const {
        return "EndOfLine";
    }
};

class WordBoundary {
public:
	template<typename Sequence>
	inline bool operator()(const Sequence &sequence, index_t begin, index_t end) const {
		return sequence.is_word_boundary(begin);
	}

    inline std::string name(const MatchContext &context) const {
        return "WordBoundary";
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

    inline std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "MatchCharacterOrExpression(";
        s << m_match_expression.name(context) << ")";
        return s.str();
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

    inline std::string name(const MatchContext &context) const {
        return "MatchToFalse";
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

    inline std::string name(const MatchContext &context) const {
        return "MatchToPattern: " + m_patt->debug(context.evaluation);
    }
};

template<typename Match>
using MatchCharacter = MatchCharacterOrExpression<Match, MatchToPattern>;

template<typename Dummy, typename MatchRest>
class SameMatcher : public ElementMatcher<MatchSame, MatchRest> {
public:
    inline SameMatcher(const BaseExpressionRef &patt, const MatchRest &next) :
	    ElementMatcher<MatchSame, MatchRest>(MatchSame(patt), next) {
    }

    DECLARE_NO_MATCH_CHARACTER_METHODS
};

template<typename Dummy, typename MatchRest>
class SameStringMatcher : public ElementMatcher<MatchSame, MatchRest> {
public:
    inline SameStringMatcher(const BaseExpressionRef &patt, const MatchRest &next) :
	    ElementMatcher<MatchSame, MatchRest>(MatchSame(patt), next) {
    }
};

template<typename Dummy, typename MatchRest>
class ExceptMatcher : public PatternMatcher {
private:
    const PatternMatcherRef m_match;
    const MatchRest m_rest;

    template<typename Sequence>
    inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

        if (begin == end) {
			return -1;
        }

        if (m_match->match(sequence, begin, begin + 1) >= 0) {
            return -1;
        } else {
			auto element = sequence.element(begin);
            return m_rest(sequence, begin + 1, end, element);
        }
    }

public:
    inline ExceptMatcher(const PatternMatcherRef &match, const MatchRest &next) :
        m_match(match), m_rest(next) {
    }

    virtual std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "ExceptMatcher(" << m_match->name(context) << "), " << m_rest.name(context);
        return s.str();
    }

    DECLARE_MATCH_METHODS
};

template<typename Dummy, typename MatchRest>
class AlternativesMatcher : public PatternMatcher {
private:
	std::vector<PatternMatcherRef> m_matchers;
    const MatchRest m_rest;

    template<typename Sequence>
    inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

	    const auto &state = sequence.context().match;
	    const size_t vars0 = state->n_slots_fixed();
	    for (const PatternMatcherRef &matcher : m_matchers) {
	        const index_t match = matcher->match(sequence, begin, end);
            if (match >= 0) {
                auto slice = sequence.slice(begin, match);
                return m_rest(sequence, match, end, slice);
            } else {
	            state->backtrack(vars0);
            }
        }

        return -1;
    }

public:
	inline AlternativesMatcher(const std::vector<PatternMatcherRef> &matchers, const MatchRest &next) :
        m_matchers(matchers), m_rest(next) {
	}

    virtual std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "AlternativesMatcher(";
        size_t i = 0;
        for (auto &m : m_matchers) {
            if (i > 0) {
                s << ", ";
            }
            s << m->name(context);
            i++;
        }
        s << "), " << m_rest.name(context);
        return s.str();
    }

    DECLARE_MATCH_METHODS
};

template<typename Dummy, typename MatchRest>
class OptionalMatcher : public PatternMatcher {
private:
	const PatternMatcherRef m_matcher;
	const BaseExpressionRef m_default;
	const MatchRest m_rest;

	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		const auto &state = sequence.context().match;
		const size_t vars0 = state->n_slots_fixed();

		const index_t match = m_matcher->match(sequence, begin, end);
		if (match >= 0) {
			auto slice = sequence.slice(begin, match);
			const index_t rest_match = m_rest(sequence, match, end, slice);
			if (rest_match >= 0) {
				return rest_match;
			}
		}

		state->backtrack(vars0);

		const index_t match1 = m_matcher->match(
			FastLeafSequence(sequence.context(), &m_default), 0, 1);
		if (match1 == 1) {
			auto slice = sequence.slice(begin, begin);
			const index_t rest_match = m_rest(sequence, begin, end, slice);
			if (rest_match >= 0) {
				return rest_match;
			}
		}

		state->backtrack(vars0);

		return -1;
	}

public:
	inline OptionalMatcher(
		const std::tuple<PatternMatcherRef, BaseExpressionRef> &parameter,
		const MatchRest &next) :
		m_matcher(std::get<0>(parameter)), m_default(std::get<1>(parameter)), m_rest(next) {
	}

	virtual std::string name(const MatchContext &context) const {
		std::ostringstream s;
		s << "OptionalMatcher(";
		s << m_matcher->name(context) << ": " << m_default->debug(context.evaluation);
		s << "), " << m_rest.name(context);
		return s.str();
	}

	DECLARE_MATCH_METHODS
};

template<typename ElementTest, typename MatchRest>
class BlankMatcher : public PatternMatcher {
private:
	const ElementTest m_test;
    const MatchRest m_rest;

	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		auto element = sequence.element(begin);
		if (begin < end && m_test(sequence, element) > begin) {
            return m_rest(sequence, begin + 1, end, element);
		} else {
			return -1;
		}
	}

public:
	inline BlankMatcher(
        const ElementTest &test,
        const MatchRest &next) :

		m_test(test),
        m_rest(next) {
	}

    virtual std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "BlankMatcher(" << m_test.name(context) << "), " << m_rest.name(context);
        return s.str();
    }

	DECLARE_MATCH_METHODS
};

template<typename ElementTest>
class Longest {
private:
	const ElementTest m_test;

public:
	inline Longest(const ElementTest &test) : m_test(test) {
	}

	template<typename... Args>
	inline index_t operator()(Args... args) const {
		return m_test.greedy(*this, args...);
	}

    inline std::string name(const MatchContext &context) const {
        return "Longest";
    }

public:
	template<typename Sequence, typename Take>
	inline index_t no_test(
		const Sequence &sequence,
	    const index_t begin,
		const index_t min_size,
		const index_t max_size,
	    const Take &take) const {

		for (index_t i = max_size; i >= min_size; i--) {
			const index_t match = take(i);
			if (match >= 0) {
				return match;
			}
		}

		return -1;
	}

	template<typename Test, typename Sequence, typename Take>
	inline index_t simple_test(
		const Test &test_element,
		const size_t match_size,
		const Sequence &sequence,
		const index_t begin,
		const index_t min_size,
		const index_t max_size,
		const Take &take) const {

		index_t n = 0;
		while (n < max_size) {
			const index_t up_to = test_element(
				sequence, begin + n, begin + max_size);
			if (up_to < 0) {
				break;
			}
			assert(up_to == begin + n + match_size);
			n += match_size;
		}

		while (n >= min_size) {
			const index_t match = take(n);
			if (match >= 0) {
				return match;
			}
			n -= match_size;
		}

		return -1;
	}

	template<typename Test, typename Sequence, typename Take>
	inline index_t complex_test(
		const Test &test_element,
		const Sequence &sequence,
		const index_t begin,
		const index_t min_size,
		const index_t max_size,
		const Take &take) const {

		std::vector<std::tuple<index_t, index_t>> states;
		auto &match = sequence.context().match;

		index_t n = 0;
		while (n < max_size) {
			const auto vars0 = match->n_slots_fixed();
			const index_t up_to = test_element(
				sequence, begin + n, begin + max_size);
			if (up_to < 0) {
				break;
			}
			n = up_to - begin;
			assert(n <= max_size);
			states.push_back(std::make_tuple(n, vars0));
		}

		for (auto i = states.rbegin(); i != states.rend(); i++) {
			const index_t n = std::get<0>(*i);
			if (n >= min_size) {
				const index_t up_to = take(n);
				if (up_to >= 0) {
					return up_to;
				}
			}
			match->backtrack(std::get<1>(*i));
		}

		return -1;
	}
};

template<typename ElementTest>
class Shortest {
private:
	const ElementTest m_test;

public:
	inline Shortest(const ElementTest &test) : m_test(test) {
	}

	template<typename... Args>
	inline index_t operator()(Args... args) const {
		return m_test.greedy(*this, args...);
	}

    inline std::string name(const MatchContext &context) const {
        return "Shortest";
    }

public:
	template<typename Sequence, typename Take>
	inline index_t no_test(
		const Sequence &sequence,
		const index_t begin,
		const index_t min_size,
		const index_t max_size,
		const Take &take) const {

		for (index_t i = min_size; i <= max_size; i++) {
			const index_t match = take(i);
			if (match >= 0) {
				return match;
			}
		}

		return -1;
	}

	template<typename Test, typename Sequence, typename Take>
	inline index_t simple_test(
		const Test &test_element,
		const size_t /*match_size*/,
		const Sequence &sequence,
		const index_t begin,
		const index_t min_size,
		const index_t max_size,
		const Take &take) const {

		index_t n = 0;
		while (n < min_size) {
			const index_t up_to = test_element(
				sequence, begin + n, begin + max_size);
			if (up_to < 0) {
				break;
			}
			n = up_to - begin;
		}

		while (n <= max_size) {
			const index_t up_to = test_element(
				sequence, begin + n, begin + max_size);
			if (up_to < 0) {
				break;
			}
			const index_t match = take(n);
			if (match >= 0) {
				return match;
			}
		}

		return -1;
	}

	template<typename Test, typename Sequence, typename... Args>
	inline index_t complex_test(
		const Test &test_element,
		const Sequence &sequence,
		const Args&... args) const {

		auto &match = sequence.context().match;
		const auto vars0 = match->n_slots_fixed();

		const index_t up_to = simple_test(test_element, 0, sequence, args...);

		if (up_to < 0) {
			match->backtrack(vars0);
		}

		return up_to;
	}
};

template<template<typename> class Strategy, index_t Minimum, typename Tests, typename MatchRest>
class BlankSequenceMatcher : public PatternMatcher {
private:
	const Strategy<Tests> m_strategy;
    const MatchRest m_rest;

	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		const index_t n = end - begin;
		const index_t max_size = n - m_size.from_next().min();

		if (max_size < Minimum) {
			return -1;
		}

		const index_t min_size = (sequence.context().options & MatchContext::NoEndAnchor) ?
			Minimum : std::max(n - index_t(m_size.from_next().max()), Minimum);

		const MatchRest &rest = m_rest;

		return m_strategy(
			sequence,
			begin,
			min_size,
			max_size,
			[begin, end, &sequence, &rest] (index_t i) {
				auto slice = sequence.slice(begin, begin + i);
                return rest(sequence, begin + i, end, slice);
			});
	}

public:
	inline BlankSequenceMatcher(
		const Tests &tests,
        const MatchRest &next) :

		m_strategy(tests),
        m_rest(next) {
	}

    virtual std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "BlankSequenceMatcher(" << m_strategy.name(context) << "), " << m_rest.name(context);
        return s.str();
    }

	DECLARE_MATCH_METHODS
};

template<typename Dummy, typename MatchRest>
class OptionsPatternMatcher : public PatternMatcher {
private:
    const MatchRest m_rest;

    template<typename Sequence>
    inline index_t do_match(
        const Sequence &sequence,
        index_t begin,
        index_t end) const {

	    if (begin == end) {
		    auto slice = sequence.slice(begin, end);
		    return m_rest(sequence, begin, end, slice);
	    }

	    return sequence.context().match->options(sequence, begin, end,
			[this, &sequence] (index_t begin, index_t t, index_t end) {
			    auto slice = sequence.slice(begin, t);
			    return m_rest(sequence, t, end, slice);
		    });
    }

public:
    inline OptionsPatternMatcher(
        const std::tuple<>,
        const MatchRest &next) :

        m_rest(next) {
    }

    virtual std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "OptionsPatternMatcher(), " << m_rest.name(context);
        return s.str();
    }

    DECLARE_MATCH_EXPRESSION_METHODS
    DECLARE_NO_MATCH_CHARACTER_METHODS
};

class PatternFactory {
private:
	CompiledVariables &m_variables;
	const BaseExpressionRef m_test;
	const SymbolRef m_variable;
	const PatternMatcherRef m_next;
	const bool m_shortest;
    const bool m_anchored;

	PatternFactory(
		CompiledVariables &variables,
		const BaseExpressionRef test,
		const SymbolRef variable,
		const PatternMatcherRef next,
		const bool shortest,
        const bool anchored) :

		m_variables(variables),
		m_test(test),
		m_variable(variable),
		m_next(next),
		m_shortest(shortest),
        m_anchored(anchored) {
	}

    template<template<typename, typename> class Matcher, typename Parameter, typename Test, typename Variable>
    PatternMatcherRef create(const Parameter &parameter, const Test &test, const Variable &variable) const {
        if (!m_anchored) {
            const auto rest = RestMatcher<Test, Variable, Unanchored>(test, variable, Unanchored());
            return new Matcher<Parameter, decltype(rest)>(parameter, rest);
        } else if (m_next) {
            const auto rest = RestMatcher<Test, Variable, Continue>(test, variable, Continue(m_next));
            return new Matcher<Parameter, decltype(rest)>(parameter, rest);
        } else {
            const auto rest = RestMatcher<Test, Variable, Terminate>(test, variable, Terminate());
            return new Matcher<Parameter, decltype(rest)>(parameter, rest);
        }
    }

    template<template<typename, typename> class Matcher, typename Parameter, typename Test>
    PatternMatcherRef create(const Parameter &parameter, const Test &test) const {
        if (m_variable) {
	        const index_t slot_index = m_variables.lookup_slot(m_variable);
            return create<Matcher>(parameter, test, AssignVariable(slot_index));
        } else {
            return create<Matcher>(parameter, test, NoVariable());
        }
    }

public:
	PatternFactory(CompiledVariables &variables) :
		m_variables(variables),
		m_shortest(false),
        m_anchored(true) {
	}

	inline const CompiledVariables &variables() const {
		return m_variables;
	}

	inline PatternMatcherRef next() const {
		return m_next;
	}

	inline bool is_shortest() const {
		return m_shortest;
	}

	PatternFactory for_variable(const SymbolRef &v) const {
		return PatternFactory(m_variables, m_test, v, m_next, m_shortest, m_anchored);
	}

	PatternFactory for_test(const BaseExpressionRef &t) const {
		return PatternFactory(m_variables, t, m_variable, m_next, m_shortest, m_anchored);
	}

	PatternFactory for_next(const PatternMatcherRef &n) const {
		return PatternFactory(m_variables, m_test, m_variable, n, m_shortest, m_anchored);
	}

	PatternFactory for_shortest() const {
		return PatternFactory(m_variables, m_test, m_variable, m_next, true, m_anchored);
	}

	PatternFactory for_longest() const {
		return PatternFactory(m_variables, m_test, m_variable, m_next, false, m_anchored);
	}

	PatternFactory stripped(bool anchored = true) const {
		return PatternFactory(m_variables, BaseExpressionRef(), SymbolRef(), PatternMatcherRef(), m_shortest, anchored);
	}

	PatternFactory alternative() const {
		return PatternFactory(m_variables, m_test, m_variable, m_next, m_shortest, true);
	}

	PatternFactory unbound() const {
		return PatternFactory(m_variables, BaseExpressionRef(), SymbolRef(), PatternMatcherRef(), m_shortest, true);
	}

	template<template<typename, typename> class Matcher, typename Parameter>
    PatternMatcherRef create(const Parameter &parameter) const {
        if (m_test) {
            return create<Matcher>(parameter, PatternTest(m_test));
        } else {
            return create<Matcher>(parameter, NoPatternTest());
        }
    }

	PatternMatcherRef create_empty() const {
		const auto rest = RestMatcher<NoPatternTest, NoVariable, Terminate>(NoPatternTest(), NoVariable(), Terminate());
		return new EmptyMatcher<decltype(rest)>(rest);
	}
};

class PatternCompiler {
private:
    const bool m_is_string_pattern;

protected:
	friend class RepeatedForm;

	PatternMatcherRef character_intrinsic_matcher(
        const BaseExpressionRef &curr,
		const PatternFactory &factory) const;

	PatternMatcherRef compile_element(
		const BaseExpressionRef &curr,
        const PatternMatcherSize &size,
        const PatternFactory &factory);

	PatternMatcherRef compile_sequence(
		const BaseExpressionRef &patt_head,
		const BaseExpressionRef *patt_begin,
		const BaseExpressionRef *patt_end,
        const PatternMatcherSize &size,
		const PatternFactory &factory);

public:
    PatternCompiler(
        bool is_string_pattern);

	PatternMatcherRef compile(
		const BaseExpressionRef *begin,
		const BaseExpressionRef *end,
        const MatchSize &size_from_here,
        const PatternFactory &factory);

	PatternMatcherRef compile(
		const BaseExpressionRef &patt,
        const MatchSize &size_from_here,
        const PatternFactory &factory);
};

PatternCompiler::PatternCompiler(bool is_string_pattern) :
    m_is_string_pattern(is_string_pattern) {
}

PatternMatcherRef PatternCompiler::character_intrinsic_matcher(
	const BaseExpressionRef &curr,
	const PatternFactory &factory) const {

	const PatternCompiler * const self = this;

	auto create_blank = [self, &curr, &factory] (const auto &test) {
		const auto match_class = MatchCharacter<MatchCharacterBlank<decltype(test)>>(
			MatchCharacterBlank<decltype(test)>(test), MatchToPattern(curr));
		return factory.create<ElementMatcher>(match_class);
	};

	auto create_blank_sequence = [self, &curr, &factory] (const auto &test) {
		const auto match_class = MatchCharacter<MatchCharacterBlankSequence<decltype(test)>>(
			MatchCharacterBlankSequence<decltype(test)>(test), MatchToPattern(curr));
		return factory.create<ElementMatcher>(match_class);
	};

	switch (curr->symbol()) {
		case S::StartOfString:
			return factory.create<StartMatcher>(nothing());

		case S::EndOfString:
			return new EndMatcher();

		case S::StartOfLine:
			return factory.create<PositionMatcher>(
				MatchCharacterPosition<StartOfLine>(StartOfLine(), MatchToFalse()));

		case S::EndOfLine:
			return factory.create<PositionMatcher>(
				MatchCharacterPosition<EndOfLine>(EndOfLine(), MatchToFalse()));

		case S::WordBoundary:
			return factory.create<PositionMatcher>(
				MatchCharacterPosition<WordBoundary>(WordBoundary(), MatchToFalse()));

		case S::DigitCharacter:
			return create_blank([] (auto p) {
				return u_isdigit(p);
			});

		case S::Whitespace:
			return create_blank_sequence([] (auto p) {
				return u_isWhitespace(p);
			});

		case S::WhitespaceCharacter:
			return create_blank([] (auto p) {
				return u_isWhitespace(p);
			});

		case S::WordCharacter:
			return create_blank([] (auto p) {
				return u_isalnum(p);
			});

		case S::LetterCharacter:
			return create_blank([] (auto p) {
				return u_isalpha(p);
			});

		case S::HexidecimalCharacter:
			return create_blank([] (auto p) {
				return u_isxdigit(p);
			});

		default:
			return PatternMatcherRef();
	}
}

PatternMatcherRef PatternCompiler::compile_element(
	const BaseExpressionRef &curr,
    const PatternMatcherSize &size,
	const PatternFactory &factory) {

	UnsafePatternMatcherRef matcher;

	switch (curr->type()) {
		case ExpressionType: {
			PatternCompiler * const compiler = this;
			const Expression * const patt_expr = curr->as_expression();

			matcher = patt_expr->with_leaves_array(
				[compiler, patt_expr, &size, &factory]
				(const BaseExpressionRef *leaves, size_t n) {
					return compiler->compile_sequence(
						patt_expr->head(), leaves, leaves + n, size, factory);
				});
			break;
		}

		case StringType:
			matcher = factory.create<SameStringMatcher>(curr);
			break;

		case SymbolType: {
			matcher = character_intrinsic_matcher(curr, factory);
			if (matcher) {
				break;
			}

			// fallthrough to generic SameMatcher
		}

		default:
			matcher = factory.create<SameMatcher>(curr);
			break;
	}

	return matcher;
}

class PatternBuilder {
private:
    CachedPatternMatcherRef m_next_matcher;

public:
    template<typename Compile>
    void add(
        const PatternMatcherSize &size,
        const PatternFactory &factory,
        const Compile &compile) {

        const PatternMatcherRef matcher =
            compile(factory.for_next(m_next_matcher));

        m_next_matcher.initialize(matcher);

        matcher->set_size(size);
    }

    PatternMatcherRef entry() {
        return m_next_matcher;
    }
};

PatternMatcherRef PatternCompiler::compile(
	const BaseExpressionRef *begin,
	const BaseExpressionRef *end,
    const MatchSize &size_from_here,
	const PatternFactory &factory) {

	const index_t n = end - begin;

	if (n == 0) {
		UnsafePatternMatcherRef matcher = factory.create_empty();
		matcher->set_size(PatternMatcherSize(size_from_here, size_from_here));
		return matcher;
	}

	std::vector<MatchSize> matchable;
	matchable.reserve(n + 1);
	MatchSize size = size_from_here;
	matchable.push_back(size);
	for (const BaseExpressionRef *i = end - 1; i >= begin; i--) {
		size += m_is_string_pattern ? (*i)->string_match_size() : (*i)->match_size();
		matchable.push_back(size);
	}
	std::reverse(matchable.begin(), matchable.end());

	UnsafePatternMatcherRef next_matcher = factory.next();

	for (index_t i = n - 1; i >= 0; i--) {
        const PatternMatcherSize size(
            matchable[i], matchable[i + 1]);

        const PatternMatcherRef matcher = compile_element(
            begin[i], size, factory.for_next(next_matcher));

		next_matcher = matcher;

		matcher->set_size(size);
    }

	next_matcher->set_variables(factory.variables());

	return next_matcher;
}

PatternMatcherRef PatternCompiler::compile(
	const BaseExpressionRef &patt,
    const MatchSize &size_from_here,
    const PatternFactory &factory) {

	if (patt->is_expression() &&
		patt->as_expression()->head()->symbol() == S::StringExpression) {

		PatternCompiler * const compiler = this;
		return patt->as_expression()->with_leaves_array(
			[&compiler, &size_from_here, &factory] (const BaseExpressionRef *leaves, size_t n) {
				return compiler->compile(leaves, leaves + n, size_from_here, factory);
			});
	} else {
		return compile(&patt, &patt + 1, size_from_here, factory);
	}
}

class BlankForm {
private:
	const BaseExpressionRef * const m_patt_begin;
	const BaseExpressionRef * const m_patt_end;

public:
	inline BlankForm(
		const BaseExpressionRef *patt_begin,
		const BaseExpressionRef *patt_end) :
		m_patt_begin(patt_begin),
		m_patt_end(patt_end) {
	}

	template<typename Make>
	inline PatternMatcherRef matcher(const Make &make) const {
		if (m_patt_end - m_patt_begin == 1) {
			const BaseExpressionRef &head = *m_patt_begin;
			if (head->is_symbol()) {
				return make(TestHead<SameSymbolHead>(head));
			} else {
				return make(TestHead<SameGenericHead>(head));
			}
		} else {
			return make(TestNone());
		}
	}
};

class RepeatedForm {
private:
	PatternCompiler * const m_compiler;
    const MatchSize &m_size_from_here;
	const PatternFactory &m_factory;
	const BaseExpressionRef * const m_patt_begin;
	const BaseExpressionRef * const m_patt_end;

public:
	inline RepeatedForm(
		PatternCompiler *compiler,
        const MatchSize &size_from_here,
		const PatternFactory &factory,
		const BaseExpressionRef *patt_begin,
		const BaseExpressionRef *patt_end) :
		m_compiler(compiler),
        m_size_from_here(size_from_here),
		m_factory(factory),
		m_patt_begin(patt_begin),
		m_patt_end(patt_end) {
	}

	template<typename Make>
	inline PatternMatcherRef matcher(const Make &make) const {
        UnsafePatternMatcherRef r_matcher;
		if (m_patt_end - m_patt_begin == 1) {
			const PatternMatcherRef matcher = m_compiler->compile(
				*m_patt_begin,
                MatchSize::exactly(0),
                m_factory.stripped(false));
			const auto fixed_size = matcher->fixed_size();
			if (fixed_size && matcher->variables().empty()) {
                r_matcher = make(TestRepeated<SimpleGreedy>(matcher, SimpleGreedy(*fixed_size)));
			} else {
                r_matcher = make(TestRepeated<ComplexGreedy>(matcher, ComplexGreedy()));
			}
		} else {
            r_matcher = make(TestNone());
		}
        r_matcher->set_size(PatternMatcherSize(m_size_from_here, m_size_from_here)); // FIXME
        return r_matcher;
	}
};

template<template<typename, typename> class Match, typename Form>
inline PatternMatcherRef create_blank_matcher(
	const Form &form,
	const PatternFactory &factory) {

	return form.matcher([&factory] (const auto &element_test) {
		return factory.create<Match>(element_test);
	});
}

template<template<template<typename> class, index_t, typename, typename> class Match, index_t Minimum>
class create_blank_sequence_matcher {
private:
	template<typename Tests, typename Variable>
	using MatchLongest = Match<Longest, Minimum, Tests, Variable>;

	template<typename Tests, typename Variable>
	using MatchShortest = Match<Shortest, Minimum, Tests, Variable>;

public:
	template<typename Form>
	inline PatternMatcherRef operator()(
		const Form &form,
		const PatternFactory &factory) const {
		if (factory.is_shortest()) {
			return create_blank_matcher<MatchShortest, Form>(form, factory);
		} else {
			return create_blank_matcher<MatchLongest, Form>(form, factory);
		}
	}
};

PatternMatcherRef PatternCompiler::compile_sequence(
	const BaseExpressionRef &patt_head,
	const BaseExpressionRef *patt_begin,
	const BaseExpressionRef *patt_end,
    const PatternMatcherSize &size,
	const PatternFactory &factory) {

	switch (patt_head->symbol()) {
		case S::Blank:
            return create_blank_matcher<BlankMatcher>(
		        BlankForm(patt_begin, patt_end), factory);

	    case S::BlankSequence:
			return create_blank_sequence_matcher<BlankSequenceMatcher, 1>()(
				BlankForm(patt_begin, patt_end), factory);

		case S::BlankNullSequence:
			return create_blank_sequence_matcher<BlankSequenceMatcher, 0>()(
				BlankForm(patt_begin, patt_end), factory);

		case S::Repeated:
			return create_blank_sequence_matcher<BlankSequenceMatcher, 1>()(
				RepeatedForm(this, size.from_here(), factory, patt_begin, patt_end), factory);

        case S::RepeatedNull:
            return create_blank_sequence_matcher<BlankSequenceMatcher, 0>()(
                RepeatedForm(this, size.from_here(), factory, patt_begin, patt_end), factory);

		case S::Alternatives: {
			std::vector<PatternMatcherRef> matchers;
			matchers.reserve(patt_end - patt_begin);
			for (const BaseExpressionRef *patt = patt_begin; patt < patt_end; patt++) {
				matchers.push_back(compile(*patt, size.from_here(), factory.alternative()));
			}
			return factory.unbound().create<AlternativesMatcher>(matchers);
		}

        case S::Except:
            if (patt_end - patt_begin == 1) { // 1 leaf ?
                return factory.create<ExceptMatcher>(
                    compile(*patt_begin, size.from_here(), factory));
            }
			break;

		case S::Shortest:
			if (patt_end - patt_begin == 1) { // 1 leaf ?
				return compile(*patt_begin, size.from_here(), factory.for_shortest());
			}
			break;

		case S::Longest:
			if (patt_end - patt_begin == 1) { // 1 leaf ?
				return compile(*patt_begin, size.from_here(), factory.for_longest());
			}
			break;

		case S::Pattern:
			if (patt_end - patt_begin == 2) { // 2 leaves?
				if ((*patt_begin)->is_symbol()) {
					const SymbolRef variable = const_pointer_cast<Symbol>(
						static_pointer_cast<const Symbol>(patt_begin[0]));
					const PatternMatcherRef matcher = compile(
						patt_begin[1], size.from_here(), factory.for_variable(variable));
					return matcher;
				}
			}
			break;

		case S::PatternTest:
			if (patt_end - patt_begin == 2) { // 2 leaves?
                return compile(*patt_begin, size.from_here(), factory.for_test(patt_begin[1]));
			}
			break;

		case S::Optional:
			if (patt_end - patt_begin == 2) { // 2 leaves?
				const PatternMatcherRef matcher = compile(
					patt_begin[0], size.from_here(), factory.stripped(false));
				return factory.create<OptionalMatcher>(std::make_tuple(matcher, patt_begin[1]));
			}
			break;

        case S::OptionsPattern: {
            return factory.create<OptionsPatternMatcher>(std::make_tuple());
        }

        default:
			break;
	}

	const PatternMatcherRef match_head = compile(
		&patt_head, &patt_head + 1, MatchSize::exactly(0), factory.stripped());

	const PatternMatcherRef match_leaves = compile(
		patt_begin, patt_end, MatchSize::exactly(0), factory.stripped());

	return factory.create<ExpressionMatcher>(
		std::make_tuple(match_head, match_leaves));
}

PatternMatcherRef compile_expression_pattern(const BaseExpressionRef &patt) {
	PatternCompiler compiler(false);
	CompiledVariables variables;
	return compiler.compile(patt, MatchSize::exactly(0), PatternFactory(variables).for_shortest());
}

PatternMatcherRef compile_string_pattern(const BaseExpressionRef &patt) {
	PatternCompiler compiler(true);
	CompiledVariables variables;
	return compiler.compile(patt, MatchSize::exactly(0), PatternFactory(variables).for_longest());
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

RewriteBaseExpression MatcherBase::prepare(const BaseExpressionRef &item) const {
    CompiledArguments arguments(
        m_matcher->variables());
    return RewriteBaseExpression::construct(
        arguments, item->as_expression());
}
