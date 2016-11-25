#include "types.h"
#include "expression.h"
#include "pattern.h"
#include "evaluate.h"

SortKey Rule::pattern_key() const {
	return SortKey(0, 0, 1, 1, 0, nullptr, 1);
}

SortKey BaseExpression::sort_key() const {
	throw std::runtime_error("not implemented");
}

SortKey BaseExpression::pattern_key() const {
	return SortKey(0, 0, 1, 1, 0, nullptr, 1);
}

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
            var = var->next_variable();
        }
    }
    s << "}";
    s << ">";
    return s;
}
