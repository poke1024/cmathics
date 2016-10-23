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

Match BaseExpression::match_sequence(const Matcher &matcher) const {
    // "this" is always pattern[0].

    auto sequence = matcher.sequence();

    if (sequence.size() == 0) {
        return Match(false);
    } else if (same(sequence[0])) {
        return matcher(1);
    } else {
        return Match(false);
    }
}

Match BaseExpression::match_sequence_with_head(const Matcher &matcher) const {
    // pattern[0] is always an Expression.
    // "this" is always pattern[0]->_head.

    auto sequence = matcher.sequence();

    if (sequence.size() == 0) {
        return Match(false);
    }

    if (!matcher.this_pattern()->same(sequence[0])) {
        return Match(false);
    }

    return matcher(1);
}
