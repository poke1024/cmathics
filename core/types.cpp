#include "types.h"
#include "pattern.h"

const char *type_name(Type type) {
    switch (type) {
	    case MachineIntegerType:
		    return "MachineInteger";
	    case BigIntegerType:
		    return "BigInteger";
	    case MachineRealType:
		    return "MachineReal";
	    case BigRealType:
		    return "BigReal";
	    case RationalType:
		    return "Rational";
	    case ComplexType:
		    return "Complex";
	    case ExpressionType:
		    return "Expression";
	    case SymbolType:
		    return "Symbol";
	    case StringType:
		    return "String";
	    default:
		    return "Unknown";
    }
}

std::ostream &operator<<(std::ostream &s, const Match &m) {
    s << "Match<" << (m ? "true" : "false"); // << ", ";
    auto var = m.variables();
    s << "{";
    if (var) {
        bool first = true;
        while (var) {
            BaseExpressionRef value = var->matched_value();
            if (!first) {
                s << ", ";
            }
            s << var->fullform() << ":" << value->fullform();
            first = false;
            var = var->linked_variable();
        }
    }
    s << "}";
    s << ">";
    return s;
}

bool BaseExpression::match_sequence(const Matcher &matcher) const {
    // "this" is always pattern[0].

    auto sequence = matcher.sequence();

    if (sequence.size() == 0) {
        return false;
    } else if (same(sequence[0])) {
        return matcher(1, nullptr);
    } else {
        return false;
    }
}

bool BaseExpression::match_sequence_with_head(ExpressionPtr patt, const Matcher &matcher) const {
    // pattern[0] is always an Expression.
    // "this" is always pattern[0]->_head.

    auto sequence = matcher.sequence();

    if (sequence.size() == 0) {
        return false;
    } else {
        auto next = sequence[0];
        if (next->type() == ExpressionType) {
            auto expr = std::static_pointer_cast<const Expression>(next);
            if (!expr->head()->same(patt->head())) {
                return false;
            } else {
                return matcher.descend();
            }
        } else {
            return false;
        }
    }
}
