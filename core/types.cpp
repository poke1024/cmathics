#include "types.h"
#include "pattern.h"

std::ostream &operator<<(std::ostream &s, const Match &m) {
    s << "Match<" << (m ? "true" : "false") << ", ";
    auto vars = m.vars();
    s << "{";
    if (vars) {
        bool first = true;
        for (auto assignment : *vars) {
            const Symbol *var;
            BaseExpressionPtr value;
            std::tie(var, value) = assignment;
            if (!first) {
                s << ", ";
            }
            s << var->fullform() << ":" << value->fullform();
            first = false;
        }
    }
    s << "}";
    s << ">";
    return s;
}

Match BaseExpression::match_sequence(
    const Slice &pattern,
    const Slice &sequence) const {

    // "this" is always pattern[0].

    if (sequence.size() == 0) {
        return Match(false);
    } else if (same(sequence[0])) {
        if (pattern.size() == 1) {
            return Match(true);
        } else {
            return pattern[0]->match_sequence(
                pattern.slice(1),
                sequence.slice(1));
        }
    } else {
        return Match(false);
    }
}

Match BaseExpression::match_sequence_with_head(
    const Slice &pattern,
    const Slice &sequence) const {

    // pattern[0] is always an Expression.
    // "this" is always pattern[0]->_head.

    if (sequence.size() == 0) {
        return Match(false);
    }

    if (!pattern[0]->same(sequence[0])) {
        return Match(false);
    }

    if (pattern.size() == 1) {
        return Match(true);
    } else {
        auto rest = pattern.slice(1);
        return rest[0]->match_sequence(rest, sequence.slice(1));
    }
}

Match BaseExpression::match_rest(
    const Slice &pattern,
    const Slice &sequence,
    size_t match_size) const
{
    if (sequence.size() == match_size) {
        return Match(true);
    }

    assert(sequence.size() > match_size);
    auto rest = pattern.slice(1);
    return rest[0]->match_sequence(rest, sequence.slice(match_size));
}