#include "types.h"
#include "expression.h"
#include "pattern.h"
#include "evaluate.h"

BaseExpressionRef exactly_n_pattern(
	const SymbolRef &head, size_t n, const Definitions &definitions) {

	const auto &Blank = definitions.symbols().Blank;
	return expression(head, [n, &Blank] (auto &storage) {
		for (size_t i = 0; i < n; i++) {
			storage << Blank;
		}
	}, n);
}

BaseExpressionRef at_least_n_pattern(
	const SymbolRef &head, size_t n, const Definitions &definitions) {

	const auto &Blank = definitions.symbols().Blank;
	const auto &BlankNullSequence = definitions.symbols().BlankNullSequence;

	return expression(head, [n, &Blank, &BlankNullSequence] (auto &storage) {
		for (size_t i = 0; i < n; i++) {
			storage << Blank;
		}
		storage << BlankNullSequence;
	}, n + 1);
}

DefinitionsPos Rule::get_definitions_pos(const Symbol *symbol) const {
	if (pattern == symbol) {
		return DefinitionsPos::Own;
	} else if (pattern->type() != ExpressionType) {
		return DefinitionsPos::None;
	} else if (pattern->head() == symbol) {
		return DefinitionsPos::Down;
	} else if (pattern->lookup_name() == symbol) {
		return DefinitionsPos::Sub;
	} else {
		return DefinitionsPos::None;
	}
}

MatchSize Rule::match_size() const {
	return pattern->match_size();
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
