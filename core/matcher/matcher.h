#pragma once

class HeadLeavesMatcher;

class FastLeafSequence;
class SlowLeafSequence;
class AsciiCharacterSequence;
class SimpleCharacterSequence;
class ComplexCharacterSequence;

class PatternMatcher : public AbstractHeapObject<PatternMatcher> {
protected:
    PatternMatcherSize m_size;
    CompiledVariables m_variables;

public:
    virtual void set_size(const PatternMatcherSize &size) {
        m_size = size;
    }

    inline void set_variables(const CompiledVariables &variables) {
        m_variables = variables;
    }

    inline const CompiledVariables &variables() const {
        return m_variables;
    }

    inline PatternMatcher() {
    }

    virtual ~PatternMatcher() {
    }

    inline bool might_match(size_t size) const {
        return m_size.from_here().contains(size);
    }

    inline optional<size_t> fixed_size() const {
        return m_size.from_here().fixed_size();
    }

    virtual const HeadLeavesMatcher *head_leaves_matcher() const {
        return nullptr;
    }

    virtual std::string name(const MatchContext &context) const = 0; // useful for debugging

    virtual index_t match(
        const FastLeafSequence &sequence,
        index_t begin,
        index_t end) const = 0;

    virtual index_t match(
        const SlowLeafSequence &sequence,
        index_t begin,
        index_t end) const = 0;

    virtual index_t match(
        const AsciiCharacterSequence &sequence,
        index_t begin,
        index_t end) const = 0;

    virtual index_t match(
        const SimpleCharacterSequence &sequence,
        index_t begin,
        index_t end) const = 0;

    virtual index_t match(
        const ComplexCharacterSequence &sequence,
        index_t begin,
        index_t end) const = 0;

    index_t match(
        MatchContext &context,
        const String *string,
        index_t begin,
        index_t end) const;
};

class Matcher;

class IllegalStringPattern {
private:
    const BaseExpressionRef m_what;
    const BaseExpressionRef m_where;

public:
    IllegalStringPattern(const BaseExpressionRef &what) : m_what(what) {
    }

    IllegalStringPattern(const BaseExpressionRef &what, const BaseExpressionRef &where) :
        m_what(what), m_where(where) {
    }

    const BaseExpressionRef &what() const {
        return m_what;
    }

    const BaseExpressionRef &where() const {
        return m_where;
    }
};

PatternMatcherRef compile_expression_pattern(const BaseExpressionRef &patt);
PatternMatcherRef compile_string_pattern(const BaseExpressionRef &patt);
