#include "core/types.h"
#include "core/atoms/symbol.h"
#include "matcher.tcc"
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
																											  \
    virtual index_t match(                                                                             		  \
		const FlatLeafSequence &sequence,															  		  \
		index_t begin,                                                                                 		  \
		index_t end) const {                                                                           	      \
		return do_match<FlatLeafSequence>(sequence, begin, end);   	            		                      \
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

#include "generic.tcc"

class MatcherConstructionFailed {
public:
    using Function = std::function<void(const Evaluation&)>;

private:
    const Function m_message;

public:
    MatcherConstructionFailed(const Function &message) : m_message(message) {
    }

    MatcherConstructionFailed(const MatcherConstructionFailed &failed) : m_message(failed.m_message) {
    }

    void message(const Evaluation& evaluation) const {
        m_message(evaluation);
    }
};

class FailedPatternMatcher : public PatternMatcher, public ExtendedHeapObject<FailedPatternMatcher>  {
private:
    const MatcherConstructionFailed m_error;

    template<typename Sequence>
    inline index_t do_match(
        const Sequence &sequence,
        index_t begin,
        index_t end) const {

        m_error.message(sequence.context().evaluation);
        return -1;
    }

public:
    FailedPatternMatcher(const MatcherConstructionFailed &e) : m_error(e) {
        const PatternMatcherSize any_size(
            MatchSize::at_least(0), MatchSize::at_least(0));

        m_size = any_size;
    }

    DECLARE_MATCH_METHODS

    virtual std::string name(const MatchContext &context) const {
        return "FailedPatternMatcher()";
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

template<typename Dummy, typename MatchRest>
class ExpressionMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<ExpressionMatcher<Dummy, MatchRest>> {

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
        const std::tuple<PatternMatcherRef, PatternMatcherVariants> &match,
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
class StartMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<StartMatcher<Dummy,MatchRest>> {

private:
	const MatchRest m_rest;

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

class EndMatcher : public PatternMatcher, public ExtendedHeapObject<EndMatcher> {
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
		return m_callback(
			strategy,
			[this] (const auto &sequence, index_t begin, index_t end) {
				return m_patt->match(sequence, begin, end);
			},
			args...);
	}
};

class NoPatternTest {
public:
	template<typename Sequence, typename Slice>
	inline bool operator()(const Sequence &sequence, Slice &slice) const {
		return true;
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
        return expression(m_test, *slice)->evaluate_or_copy(sequence.context().evaluation)->is_true();
	}
};

template<typename F>
auto with_pattern_test(const BaseExpressionRef &test, const F& f) {
	if (test) {
		switch (test->symbol()) {
			case S::NumberQ:
				return f([] (const auto&, auto &slice) {
					return (*slice)->is_number();
				});
			case S::Positive:
				return f([] (const auto&, auto &slice) {
					return (*slice)->is_positive();
				});
			case S::Negative:
				return f([] (const auto&, auto &slice) {
					return (*slice)->is_negative();
				});
			case S::NonPositive:
				return f([] (const auto&, auto &slice) {
					return (*slice)->is_non_positive();
				});
			case S::NonNegative:
				return f([] (const auto&, auto &slice) {
					return (*slice)->is_non_negative();
				});
			default:
				return f(PatternTest(test));
		}
	} else {
		return f(NoPatternTest());
	}
}

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
class EmptyMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<EmptyMatcher<MatchRest>> {

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
class ElementMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<ElementMatcher<Match, MatchRest>> {

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
class PositionMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<PositionMatcher<Match, MatchRest>> {

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
		const auto test_code_point = [this] (auto p) {
			return m_test(p);
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
		const auto test_code_point = [this] (auto p) {
			return m_test(p);
		};

		index_t up_to = begin;
        while (up_to < end && sequence.all_code_points(up_to, test_code_point)) {
			up_to++;
		}

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

    inline auto operator()(const FlatLeafSequence &sequence, index_t begin, index_t end) const {
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
class ExceptMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<ExceptMatcher<Dummy, MatchRest>> {

private:
    const PatternMatcherRef m_except;
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

        if (m_match) {
            if (m_match->match(sequence, begin, begin + 1) < 0) {
                return -1;
            }
        }

        if (m_except->match(sequence, begin, begin + 1) >= 0) {
            return -1;
        } else {
			auto element = sequence.element(begin);
            return m_rest(sequence, begin + 1, end, element);
        }
    }

public:
    inline ExceptMatcher(const PatternMatcherRef &except, const MatchRest &next) :
        m_except(except), m_match(PatternMatcherRef()), m_rest(next) {
    }

    inline ExceptMatcher(const std::pair<PatternMatcherRef, PatternMatcherRef> &match, const MatchRest &next) :
        m_except(match.first), m_match(match.second), m_rest(next) {
    }

    virtual std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "ExceptMatcher(" << m_except->name(context);
        if (m_match) {
            s << ", " << m_match->name(context);
        }
        s << "), " << m_rest.name(context);
        return s.str();
    }

    DECLARE_MATCH_METHODS
};

template<typename Dummy, typename MatchRest>
class AlternativesMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<AlternativesMatcher<Dummy, MatchRest>> {

private:
	const std::vector<PatternMatcherRef> m_matchers;
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
	inline AlternativesMatcher(std::vector<PatternMatcherRef> &&matchers, const MatchRest &next) :
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

using SymbolSet = std::unordered_set<SymbolRef, SymbolHash, SymbolEqual>;

template<typename Dummy, typename MatchRest>
class SymbolSetMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<SymbolSetMatcher<Dummy, MatchRest>> {

private:
	const SymbolSet m_symbols;
	const MatchRest m_rest;

	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		if (begin >= end) {
			return -1;
		}

		auto element = sequence.element(begin);

		const auto item = *element;
		if (!item->is_symbol()) {
			return -1;
		}

		if (m_symbols.find(item->as_symbol()) != m_symbols.end()) {
			return m_rest(sequence, begin + 1, end, element);
		} else {
			return -1;
		}
	}

public:
	inline SymbolSetMatcher(SymbolSet &&symbols, const MatchRest &next) :
		m_symbols(symbols), m_rest(next) {
	}

	virtual std::string name(const MatchContext &context) const {
		std::ostringstream s;
		s << "SymbolSetMatcher(";
		size_t i = 0;
		for (auto symbol : m_symbols) {
			if (i > 0) {
				s << ", ";
			}
			s << symbol->name();
			i++;
		}
		s << "), " << m_rest.name(context);
		return s.str();
	}

	DECLARE_MATCH_EXPRESSION_METHODS                                                                          \
	DECLARE_NO_MATCH_CHARACTER_METHODS
};

template<typename Dummy, typename MatchRest, bool Shortest>
class OptionalMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<OptionalMatcher<Dummy, MatchRest, Shortest>> {

private:
	const PatternMatcherRef m_matcher;
	const BaseExpressionRef m_default;
	const MatchRest m_rest;

	template<typename Sequence>
	inline index_t match_default(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		const index_t match = m_matcher->match(
			FastLeafSequence(sequence.context(), nullptr, &m_default), 0, 1);
		if (match == 1) {
			auto slice = sequence.slice(begin, begin);
			return m_rest(sequence, begin, end, slice);
		} else {
			return -1;
		}
	}

	template<typename Sequence>
	inline index_t match_optional(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		const index_t match = m_matcher->match(sequence, begin, end);
		if (match >= 0) {
			auto slice = sequence.slice(begin, match);
			return m_rest(sequence, match, end, slice);
		} else {
			return -1;
		}
	}

	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		const auto &state = sequence.context().match;
		const size_t vars0 = state->n_slots_fixed();

		const index_t m1 = Shortest ?
            match_default(sequence, begin, end) :
            match_optional(sequence, begin, end);
		if (m1 >= 0) {
			return m1;
		}
		state->backtrack(vars0);

		const index_t m2 = Shortest ?
			match_optional(sequence, begin, end) :
			match_default(sequence, begin, end);
		if (m2 >= 0) {
			return m2;
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

template<typename Dummy, typename MatchRest>
using ShortestOptionalMatcher = OptionalMatcher<Dummy, MatchRest, true>;

template<typename Dummy, typename MatchRest>
using LongestOptionalMatcher = OptionalMatcher<Dummy, MatchRest, false>;

template<typename ElementTest, typename MatchRest>
class BlankMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<BlankMatcher<ElementTest, MatchRest>> {

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
			n = std::min(min_size, up_to - begin);
		}

		while (n <= max_size) {
            const index_t match = take(n);
            if (match >= 0) {
                return match;
            }
            if (n < max_size) {
                const index_t up_to = test_element(
                    sequence, begin + n, begin + max_size);
                if (up_to < 0) {
                    break;
                }
                n = up_to - begin;
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

		const index_t up_to = simple_test(
            test_element, 0, sequence, args...);

		if (up_to < 0) {
			match->backtrack(vars0);
		}

		return up_to;
	}
};

class NoLimits {
public:
    inline bool operator()(index_t &min_size, index_t &max_size) const {
        return true;
    }

    std::string name() const {
        return "no limits";
    }
};

class Limits { // for Repeated
private:
    const machine_integer_t m_min_size;
    const machine_integer_t m_max_size;

public:
    inline Limits(const MatchSize &size) :
        m_min_size(size.min()), m_max_size(size.max()) {
    }

    inline Limits(const Limits &limits) :
        m_min_size(limits.m_min_size), m_max_size(limits.m_max_size) {
    }

    inline bool operator()(index_t &min_size, index_t &max_size) const {
        min_size = std::max(min_size, m_min_size);
        max_size = std::min(max_size, m_max_size);
        return min_size <= max_size;
    }

    std::string name() const {
        std::ostringstream s;
        s << "(" << m_min_size << ", " << m_max_size << ")";
        return s.str();
    }
};

optional<MatchSize> parse_repeated_size(
    const BaseExpressionRef &spec,
    machine_integer_t min_size) {

    if (spec->is_list()) {
        const ExpressionPtr list = spec->as_expression();

        switch (list->size()) {
            case 1: {
                const auto value =
                    list->n_leaves<1>()[0]->get_machine_int_value();
                if (value) {
                    return MatchSize::between(std::max(min_size, *value), std::max(min_size, *value));
                }
                return optional<MatchSize>();
            }

            case 2: {
                const auto min =
                    list->n_leaves<2>()[0]->get_machine_int_value();
                const auto max =
                    list->n_leaves<2>()[1]->get_machine_int_value();
                if (min && max) {
                    return MatchSize::between(std::max(min_size, *min), std::max(min_size, *max));
                }
                return optional<MatchSize>();
            }

            default:
                return optional<MatchSize>();
        }
    }

    const auto value = spec->get_machine_int_value();
    if (value) {
        return MatchSize::between(std::max(min_size, machine_integer_t(1)), std::max(min_size, *value));
    }

    return optional<MatchSize>();
}

template<template<typename> class Strategy, index_t Minimum, typename Tests, typename Limits, typename MatchRest>
class SequenceMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<SequenceMatcher<Strategy, Minimum, Tests, Limits, MatchRest>> {
private:
	const Strategy<Tests> m_strategy;
    const Limits m_limits;
    const MatchRest m_rest;

	template<typename Sequence>
	inline index_t do_match(
		const Sequence &sequence,
		index_t begin,
		index_t end) const {

		const index_t n = end - begin;

		index_t max_size = n - m_size.from_next().min();

		if (max_size < Minimum) {
			return -1;
		}

		index_t min_size = (sequence.context().options & MatchContext::NoEndAnchor) ?
			Minimum : std::max(n - index_t(m_size.from_next().max()), Minimum);

        if (!m_limits(min_size, max_size)) {
            return -1;
        }

		return m_strategy(
			sequence,
			begin,
			min_size,
			max_size,
			[this, begin, end, &sequence] (index_t i) {
				auto slice = sequence.slice(begin, begin + i);
                return m_rest(sequence, begin + i, end, slice);
			});
	}

public:
	inline SequenceMatcher(
		const std::tuple<Tests, Limits> &parameters,
        const MatchRest &next) :

		m_strategy(std::get<0>(parameters)),
        m_limits(std::get<1>(parameters)),
        m_rest(next) {
	}

    virtual std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "SequenceMatcher(" << m_strategy.name(context) << ", ";
        s << Minimum << ", " << m_limits.name() << "), " << m_rest.name(context);
        return s.str();
    }

	DECLARE_MATCH_METHODS
};

template<typename Dummy, typename MatchRest>
class OptionsPatternMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<OptionsPatternMatcher<Dummy, MatchRest>> {

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

template<typename Dummy, typename MatchRest>
class ConditionMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<ConditionMatcher<Dummy, MatchRest>> {
private:
    const PatternMatcherRef m_matcher;
    const BaseExpressionRef m_condition;
    const MatchRest m_rest;

    template<typename Sequence>
    inline index_t do_match(
        const Sequence &sequence,
        index_t begin,
        index_t end) const {

        const index_t match = m_matcher->match(sequence, begin, end);
        if (match < 0) {
            return -1;
        }

        const MatchContext &context = sequence.context();
        const Evaluation &evaluation = context.evaluation;

        const BaseExpressionRef condition =
            m_condition->replace_all_or_copy(context.match, evaluation);

        if (!condition->evaluate_or_copy(evaluation)->is_true()) {
            return -1;
        }

        auto slice = sequence.slice(match, end);
        return m_rest(sequence, match, end, slice);
    }

public:
    ConditionMatcher(
        const std::tuple<PatternMatcherRef, BaseExpressionRef> &parameters,
        const MatchRest &rest) :

        m_matcher(std::get<0>(parameters)),
        m_condition(std::get<1>(parameters)),
        m_rest(rest) {
    }

    virtual std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "ConditionMatcher(" << m_matcher->name(context) << ", ";
        s << m_condition->debugform() << "), " << m_rest.name(context);
        return s.str();
    }

    DECLARE_MATCH_METHODS
};

class PatternLength {
public:
	enum Length {
		Shortest,
		Longest,
		Fallback
	};

private:
	Length m_fallback;
	Length m_local;

public:
	inline PatternLength(const PatternLength &length) :
		m_fallback(length.m_fallback), m_local(length.m_local) {

	}

	inline PatternLength(Length fallback) :
		m_fallback(fallback), m_local(Fallback) {
	}

	inline PatternLength(Length fallback, Length local) :
		m_fallback(fallback), m_local(local) {
	}

	inline PatternLength longest() const {
		return PatternLength(m_fallback, Longest);
	}

	inline PatternLength shortest() const {
		return PatternLength(m_fallback, Shortest);
	}

	inline bool is_shortest() const {
		return m_local == Shortest || (m_local == Fallback && m_fallback == Shortest);
	}

	inline bool is_optional_shortest() const {
		return m_local == Shortest; // only if explicitly set
	}
};


class PatternFactory {
private:
	CompiledVariables &m_variables;
	const BaseExpressionRef m_test;
	const SymbolRef m_variable;
	const PatternMatcherRef m_next;
	const PatternLength m_length;
    const bool m_anchored;

	PatternFactory(
		CompiledVariables &variables,
		const BaseExpressionRef test,
		const SymbolRef variable,
		const PatternMatcherRef next,
		const PatternLength &length,
        const bool anchored) :

		m_variables(variables),
		m_test(test),
		m_variable(variable),
		m_next(next),
		m_length(length),
        m_anchored(anchored) {
	}

    template<template<typename, typename> class Matcher, typename Parameter, typename Test, typename Variable>
    PatternMatcherRef create(Parameter &&parameter, const Test &test, const Variable &variable) const {
        if (!m_anchored) {
            const auto rest = RestMatcher<Test, Variable, Unanchored>(test, variable, Unanchored());
            return Matcher<typename std::remove_reference<Parameter>::type, decltype(rest)>::construct(
		        std::forward<Parameter>(parameter), rest);
        } else if (m_next) {
            const auto rest = RestMatcher<Test, Variable, Continue>(test, variable, Continue(m_next));
            return Matcher<typename std::remove_reference<Parameter>::type, decltype(rest)>::construct(
		        std::forward<Parameter>(parameter), rest);
        } else {
            const auto rest = RestMatcher<Test, Variable, Terminate>(test, variable, Terminate());
            return Matcher<typename std::remove_reference<Parameter>::type, decltype(rest)>::construct(
		        std::forward<Parameter>(parameter), rest);
        }
    }

    template<template<typename, typename> class Matcher, typename Parameter, typename Test>
    PatternMatcherRef create(Parameter &&parameter, const Test &test) const {
        if (m_variable) {
	        const index_t slot_index = m_variables.lookup_slot(m_variable);
            return create<Matcher>(std::forward<Parameter>(parameter), test, AssignVariable(slot_index));
        } else {
            return create<Matcher>(std::forward<Parameter>(parameter), test, NoVariable());
        }
    }

public:
	PatternFactory(CompiledVariables &variables, const PatternLength &length) :
		m_variables(variables),
		m_length(length),
        m_anchored(true) {
	}

	inline const CompiledVariables &variables() const {
		return m_variables;
	}

	inline PatternMatcherRef next() const {
		return m_next;
	}

	inline const PatternLength &length() const {
		return m_length;
	}

	PatternFactory for_variable(const SymbolRef &v) const {
		return PatternFactory(m_variables, m_test, v, m_next, m_length, m_anchored);
	}

	PatternFactory for_test(const BaseExpressionRef &t) const {
		return PatternFactory(m_variables, t, m_variable, m_next, m_length, m_anchored);
	}

	PatternFactory for_next(const PatternMatcherRef &n) const {
		return PatternFactory(m_variables, m_test, m_variable, n, m_length, m_anchored);
	}

    PatternFactory unanchored() const {
        return PatternFactory(m_variables, m_test, m_variable, m_next, m_length, false);
    }

    PatternFactory for_shortest() const {
		return PatternFactory(m_variables, m_test, m_variable, m_next, m_length.shortest(), m_anchored);
	}

	PatternFactory for_longest() const {
		return PatternFactory(m_variables, m_test, m_variable, m_next, m_length.longest(), m_anchored);
	}

	PatternFactory stripped(bool anchored = true) const {
		return PatternFactory(m_variables, BaseExpressionRef(), SymbolRef(), PatternMatcherRef(), m_length, anchored);
	}

	PatternFactory alternative() const {
		return PatternFactory(m_variables, m_test, m_variable, m_next, m_length, true);
	}

	PatternFactory unbound() const {
		return PatternFactory(m_variables, BaseExpressionRef(), SymbolRef(), PatternMatcherRef(), m_length, true);
	}

	template<template<typename, typename> class Matcher, typename Parameter>
	PatternMatcherRef create(Parameter &&parameter) const {
		return with_pattern_test(m_test, [this, &parameter] (auto &&test) {
			return create<Matcher>(std::forward<Parameter>(parameter), test);
		});
	}

	PatternMatcherRef create_empty() const {
		const auto rest = RestMatcher<NoPatternTest, NoVariable, Terminate>(NoPatternTest(), NoVariable(), Terminate());
		return EmptyMatcher<decltype(rest)>::construct(rest);
	}
};

class PatternCompiler {
private:
    const BaseExpressionRef m_pattern;
    const bool m_is_string_pattern;

protected:
    template<typename Limits>
	friend class RepeatedForm;

	PatternMatcherRef character_intrinsic_matcher(
        const BaseExpressionRef &curr,
		const PatternFactory &factory) const;

	PatternMatcherRef compile_element(
		const BaseExpressionRef &curr,
        const PatternMatcherSize &size,
        const PatternFactory &factory);

	PatternMatcherRef compile_expression(
		const BaseExpressionRef &patt_head,
		const BaseExpressionRef *patt_begin,
		const BaseExpressionRef *patt_end,
        const PatternMatcherSize &size,
		const PatternFactory &factory);

    template<index_t Minimum>
    PatternMatcherRef create_repeated(
        const BaseExpressionRef &patt_head,
        const BaseExpressionRef *patt_begin,
        const BaseExpressionRef *patt_end,
        const PatternMatcherSize &size,
        const PatternFactory &factory);

    ExpressionRef full_expression(
        const BaseExpressionRef &patt_head,
        const BaseExpressionRef *patt_begin,
        const BaseExpressionRef *patt_end) const;

public:
    PatternCompiler(
        const BaseExpressionRef &pattern,
        bool is_string_pattern);

	PatternMatcherRef compile_ordered(
		const BaseExpressionRef *begin,
		const BaseExpressionRef *end,
        const MatchSize &size_of_end,
        const PatternFactory &factory);

    PatternMatcherRef compile_generic(
        const BaseExpressionRef *begin,
        const BaseExpressionRef *end,
        const MatchSize &size_from_here,
        const PatternFactory &factory);

	PatternMatcherRef compile(
		const BaseExpressionRef &patt,
        const MatchSize &size_from_here,
        const PatternFactory &factory);

    PatternMatcherRef compile(
        const PatternFactory &factory);
};

PatternCompiler::PatternCompiler(const BaseExpressionRef &pattern, bool is_string_pattern) :
    m_pattern(pattern), m_is_string_pattern(is_string_pattern) {
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
			return EndMatcher::construct();

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
					return compiler->compile_expression(
						patt_expr->head(), leaves, leaves + n, size, factory);
				});
			break;
		}

		case StringType:
			matcher = factory.create<SameStringMatcher>(curr);
			break;

		case SymbolType:
			matcher = character_intrinsic_matcher(curr, factory);
			if (matcher) {
				break;
			}

			// fallthrough to generic SameMatcher

		default:
            if (m_is_string_pattern) {
                const BaseExpressionRef pattern = m_pattern;
                const BaseExpressionRef expr = curr; // capture by value, not reference

                throw MatcherConstructionFailed([expr, pattern] (const Evaluation &evaluation) {
                    throw IllegalStringPattern(expr, pattern);
                });
            }

			matcher = factory.create<SameMatcher>(curr);
			break;
	}

    matcher->set_size(size);

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

PatternMatcherRef PatternCompiler::compile_ordered(
	const BaseExpressionRef *begin,
	const BaseExpressionRef *end,
    const MatchSize &size_of_end,
	const PatternFactory &factory) {

	const index_t n = end - begin;

	if (n == 0) {
		UnsafePatternMatcherRef matcher = factory.create_empty();
		matcher->set_size(PatternMatcherSize(size_of_end, size_of_end));
		return matcher;
	}

	std::vector<MatchSize> matchable;
	matchable.reserve(n + 1);
	MatchSize size = size_of_end;
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
    }

	next_matcher->set_variables(factory.variables());

	return next_matcher;
}

PatternMatcherRef PatternCompiler::compile_generic(
    const BaseExpressionRef *begin,
    const BaseExpressionRef *end,
    const MatchSize &size_from_here,
    const PatternFactory &factory) {

    const index_t n = end - begin;
    std::vector<PatternMatcherRef> matchers;
    matchers.reserve(n);

    const auto unanchored = factory.unanchored();

    const PatternMatcherSize any_size(
        MatchSize::at_least(0), MatchSize::at_least(0));

    for (index_t i = 0; i < n; i++) {
        matchers.push_back(compile_element(
            begin[i], any_size, unanchored));
    }

    PatternMatcherRef matcher =
        factory.create<GenericPatternMatcher>(std::move(matchers));

    return matcher;
}

PatternMatcherRef PatternCompiler::compile(
    const PatternFactory &factory) {

    try {
        return compile(m_pattern, MatchSize::exactly(0), factory);
    } catch (const MatcherConstructionFailed &e) {
        return FailedPatternMatcher::construct(e);
    }
}

PatternMatcherRef PatternCompiler::compile(
	const BaseExpressionRef &patt,
    const MatchSize &size_from_here,
    const PatternFactory &factory) {

	if (patt->is_expression() &&
		patt->as_expression()->head()->symbol() == S::StringExpression) {
		return patt->as_expression()->with_leaves_array(
			[this, &size_from_here, &factory] (const BaseExpressionRef *leaves, size_t n) {
				return compile_ordered(leaves, leaves + n, size_from_here, factory);
			});
	} else {
		return compile_ordered(&patt, &patt + 1, size_from_here, factory);
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
				return make(TestHead<SameSymbolHead>(head), NoLimits());
			} else {
				return make(TestHead<SameGenericHead>(head), NoLimits());
			}
		} else {
			return make(TestNone(), NoLimits());
		}
	}
};

template<typename Limits>
class RepeatedForm {
private:
	PatternCompiler * const m_compiler;
    const MatchSize &m_size_from_here;
	const PatternFactory &m_factory;
	const BaseExpressionRef m_pattern;
    const Limits &m_limits;

public:
	inline RepeatedForm(
		PatternCompiler *compiler,
        const MatchSize &size_from_here,
		const PatternFactory &factory,
        const BaseExpressionRef &pattern,
        const Limits &limits) :

		m_compiler(compiler),
        m_size_from_here(size_from_here),
		m_factory(factory),
		m_pattern(pattern),
        m_limits(limits) {
	}

	template<typename Make>
	inline PatternMatcherRef matcher(const Make &make) const {
        UnsafePatternMatcherRef r_matcher;
		if (m_pattern) {
			const PatternMatcherRef matcher = m_compiler->compile(
                m_pattern,
                MatchSize::exactly(0),
                m_factory.stripped(false));

			const auto fixed_size = matcher->fixed_size();

            if (fixed_size && matcher->variables().empty()) {
                r_matcher = make(TestRepeated<SimpleGreedy>(
                    matcher, SimpleGreedy(*fixed_size)), m_limits);
			} else {
                r_matcher = make(TestRepeated<ComplexGreedy>(
                    matcher, ComplexGreedy()), m_limits);
			}
		} else {
            r_matcher = make(TestNone(), m_limits);
		}

        r_matcher->set_size(PatternMatcherSize(m_size_from_here, m_size_from_here)); // FIXME
        return r_matcher;
	}
};

template<template<typename, typename> class Match, typename Form>
inline PatternMatcherRef create_blank_matcher(
	const Form &form,
	const PatternFactory &factory) {

	return form.matcher([&factory] (const auto &element_test, const NoLimits&) {
		return factory.create<Match>(element_test);
	});
}

template<typename Limits, index_t Minimum>
class create_sequence_matcher {
private:
    template<typename Tuple, typename Variable>
    using MatchLongest = SequenceMatcher<
        Longest, Minimum, typename std::tuple_element<0, Tuple>::type, typename std::tuple_element<1, Tuple>::type, Variable>;

    template<typename Tuple, typename Variable>
    using MatchShortest = SequenceMatcher<
        Shortest, Minimum, typename std::tuple_element<0, Tuple>::type, typename std::tuple_element<1, Tuple>::type, Variable>;

public:
    template<typename Form>
    inline PatternMatcherRef operator()(
        const Form &form,
        const PatternFactory &factory) const {

        if (factory.length().is_shortest()) {
            return form.matcher([&factory] (const auto &element_test, const Limits &limits) {
                return factory.create<MatchShortest>(std::make_tuple(element_test, limits));
            });
        } else {
            return form.matcher([&factory] (const auto &element_test, const Limits &limits) {
                return factory.create<MatchLongest>(std::make_tuple(element_test, limits));
            });
        }
    }
};

template<index_t Minimum>
PatternMatcherRef PatternCompiler::create_repeated(
    const BaseExpressionRef &patt_head,
    const BaseExpressionRef *patt_begin,
    const BaseExpressionRef *patt_end,
    const PatternMatcherSize &size,
    const PatternFactory &factory) {

    switch (patt_end - patt_begin) {
        case 0:
            return create_sequence_matcher<NoLimits, Minimum>()(RepeatedForm<NoLimits>(
                this, size.from_here(), factory, BaseExpressionRef(), NoLimits()), factory);

        case 1:
            return create_sequence_matcher<NoLimits, Minimum>()(RepeatedForm<NoLimits>(
                this, size.from_here(), factory, *patt_begin, NoLimits()), factory);

        case 2: {
            const optional<MatchSize> repeated_size(parse_repeated_size(patt_begin[1], Minimum));

            if (!repeated_size) {
                const ExpressionRef full = full_expression(patt_head, patt_begin, patt_end);

                throw MatcherConstructionFailed([full] (const Evaluation &evaluation) {
                    evaluation.message(
                        evaluation.Repeated,
                        "range",
                        MachineInteger::construct(2),
                        full);
                });
            }

            return create_sequence_matcher<Limits, Minimum>()(RepeatedForm<Limits>(
                this, size.from_here(), factory, patt_begin[0], Limits(*repeated_size)), factory);
        }

        default:
            return PatternMatcherRef();
    }
}

ExpressionRef PatternCompiler::full_expression(
    const BaseExpressionRef &patt_head,
    const BaseExpressionRef *patt_begin,
    const BaseExpressionRef *patt_end) const {

    return expression(patt_head, sequential([patt_begin, patt_end] (auto &store) {
        for (const BaseExpressionRef *leaf = patt_begin; leaf != patt_end; leaf++) {
            store(BaseExpressionRef(*leaf));
        }
    }));
}

PatternMatcherRef PatternCompiler::compile_expression(
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
			return create_sequence_matcher<NoLimits, 1>()(
				BlankForm(patt_begin, patt_end), factory);

		case S::BlankNullSequence:
			return create_sequence_matcher<NoLimits, 0>()(
				BlankForm(patt_begin, patt_end), factory);

		case S::Repeated: {
            const PatternMatcherRef matcher =
                create_repeated<1>(patt_head, patt_begin, patt_end, size, factory);
            if (matcher) {
                return matcher;
            }
            break;
        }

        case S::RepeatedNull: {
            const PatternMatcherRef matcher =
                create_repeated<0>(patt_head, patt_begin, patt_end, size, factory);
            if (matcher) {
                return matcher;
            }
            break;
        }

		case S::Alternatives: {
            bool all_symbols;

            if (m_is_string_pattern) {
                all_symbols = false;
            } else {
                all_symbols = true;

                for (const BaseExpressionRef *patt = patt_begin; patt < patt_end; patt++) {
                    if (!(*patt)->is_symbol()) {
                        all_symbols = false;
                        break;
                    }
                }
            }

			if (!all_symbols) {
				std::vector<PatternMatcherRef> matchers;
				matchers.reserve(patt_end - patt_begin);

				for (const BaseExpressionRef *patt = patt_begin; patt < patt_end; patt++) {
					const PatternMatcherRef matcher =
						compile(*patt, size.from_next(), factory.alternative());
					matchers.push_back(matcher);
				}

				return factory.unbound().create<AlternativesMatcher>(std::move(matchers));
			} else {
				SymbolSet symbols;
				symbols.reserve(patt_end - patt_begin);

				for (const BaseExpressionRef *patt = patt_begin; patt < patt_end; patt++) {
					symbols.insert((*patt)->as_symbol());
				}

				return factory.create<SymbolSetMatcher>(std::move(symbols));
			}
		}

        case S::Except:
            switch (patt_end - patt_begin) {
                case 1:
                    return factory.create<ExceptMatcher>(
                       compile(*patt_begin, size.from_next(), factory));
                case 2:
                    return factory.create<ExceptMatcher>(std::make_pair(
                        compile(patt_begin[0], size.from_next(), factory),
                        compile(patt_begin[1], size.from_next(), factory)));

                default:
                    break;
            }
			break;

		case S::Shortest:
			if (patt_end - patt_begin == 1) { // 1 leaf ?
				return compile(*patt_begin, size.from_next(), factory.for_shortest());
			}
			break;

		case S::Longest:
			if (patt_end - patt_begin == 1) { // 1 leaf ?
				return compile(*patt_begin, size.from_next(), factory.for_longest());
			}
			break;

		case S::Pattern:
			if (patt_end - patt_begin == 2) { // 2 leaves?
				if ((*patt_begin)->is_symbol()) {
					const SymbolRef variable = const_pointer_cast<Symbol>(
						static_pointer_cast<const Symbol>(patt_begin[0]));
					const PatternMatcherRef matcher = compile(
						patt_begin[1], size.from_next(), factory.for_variable(variable));
					return matcher;
				}
			}
			break;

		case S::PatternTest:
			if (patt_end - patt_begin == 2) { // 2 leaves?
                return compile(*patt_begin, size.from_next(), factory.for_test(patt_begin[1]));
			}
			break;

        case S::Condition:
            if (patt_end - patt_begin == 2) {
                return factory.create<ConditionMatcher>(std::make_tuple(
                    compile(*patt_begin, size.from_next(), factory), patt_begin[1]));
            }
            break;

		case S::Optional:
			if (patt_end - patt_begin == 2) { // 2 leaves?
				const PatternMatcherRef matcher = compile(
					patt_begin[0], size.from_next(), factory.stripped(false));
				const auto parameters = std::make_tuple(matcher, patt_begin[1]);
				if (factory.length().is_optional_shortest()) {
					return factory.create<ShortestOptionalMatcher>(parameters);
				} else {
					return factory.create<LongestOptionalMatcher>(parameters);
				}
			}
			break;

		case S::Verbatim:
			if (patt_end - patt_begin == 1) { // 1 leaf?
				return factory.create<SameMatcher>(patt_begin[0]);
			}
			break;

        case S::OptionsPattern:
            return factory.create<OptionsPatternMatcher>(std::make_tuple());

		case S::HoldPattern:
			if (patt_end - patt_begin == 1) { // 1 leaf?
				return compile(*patt_begin, size.from_next(), factory);
			}
			break;

        default:
			break;
	}

    if (m_is_string_pattern) {
        const BaseExpressionRef expr = full_expression(patt_head, patt_begin, patt_end);
        const BaseExpressionRef pattern = m_pattern;

        throw MatcherConstructionFailed([expr, pattern] (const Evaluation &evaluation) {
            throw IllegalStringPattern(expr, pattern);
        });
    }

    const PatternMatcherRef match_head = compile_ordered(
		&patt_head, &patt_head + 1, MatchSize::exactly(0), factory.stripped());

    const PatternMatcherRef match_leaves_ordered = compile_ordered(
        patt_begin, patt_end, MatchSize::exactly(0), factory.stripped());

    const PatternMatcherRef match_leaves_generic = compile_generic(
        patt_begin, patt_end, MatchSize::exactly(0), factory.stripped());

    PatternMatcherVariants match_leaves(
        match_leaves_ordered, match_leaves_generic);
	return factory.create<ExpressionMatcher>(
		std::make_tuple(match_head, match_leaves));
}

PatternMatcherRef compile_expression_pattern(const BaseExpressionRef &patt) {
    PatternCompiler compiler(patt, false);
    CompiledVariables variables;
    return compiler.compile(
        PatternFactory(variables, PatternLength(PatternLength::Shortest)));
}

PatternMatcherRef compile_string_pattern(const BaseExpressionRef &patt) {
    PatternCompiler compiler(patt, true);
    CompiledVariables variables;
    return compiler.compile(
        PatternFactory(variables, PatternLength(PatternLength::Longest)));
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

RewriteBaseExpression MatcherBase::prepare(
    const BaseExpressionRef &item,
    const Evaluation &evaluation) const {

    CompiledArguments arguments(
        m_matcher->variables());
    return RewriteBaseExpression::from_arguments(
        arguments, item->as_expression(), evaluation);
}

template<bool MatchHead>
inline bool HeadLeavesMatcher::match(
    MatchContext &context,
    const Expression *expr) const {

    const BaseExpressionPtr head = expr->head();
    const Attributes attributes = head->lookup_name()->state().attributes();
    const PatternMatcherRef &match_leaves = m_match_leaves(attributes);

    if (!match_leaves->might_match(expr->size())) {
        return false;
    }

    if (MatchHead) {
        const BaseExpressionRef head_ref(head);

        if (m_match_head->match(FastLeafSequence(context, nullptr, &head_ref), 0, 1) < 0) {
            return false;
        }
    }

    if (expr->has_leaves_array()) {
        if (expr->with_leaves_array(
            [&context, head, &match_leaves] (const BaseExpressionRef *leaves, size_t size) {
                return match_leaves->match(FastLeafSequence(context, head, leaves), 0, size);
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

bool HeadLeavesMatcher::with_head(
    MatchContext &context,
    const Expression *expr) const {

    return match<true>(context, expr);
}

bool HeadLeavesMatcher::without_head(
    MatchContext &context,
    const Expression *expr) const {

    return match<false>(context, expr);
}
