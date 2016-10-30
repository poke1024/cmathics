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
            BaseExpressionRef value;
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
    } else if (same(sequence[0].get())) {
        return matcher(1, nullptr);
    } else {
        return Match(false);
    }
}

Match BaseExpression::match_sequence_with_head(ExpressionPtr patt, const Matcher &matcher) const {
    // pattern[0] is always an Expression.
    // "this" is always pattern[0]->_head.

    auto sequence = matcher.sequence();

    if (sequence.size() == 0) {
        return Match(false);
    } else if (!patt->same(sequence[0].get())) {
        return Match(false);
    } else {
        return matcher(1, nullptr);
    }
}
