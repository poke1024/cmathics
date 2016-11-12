#include "types.h"
#include "pattern.h"
#include "evaluate.h"

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

BaseExpressionRef Expression::evaluated_form(const BaseExpressionRef &self, const Evaluation &evaluation) const {
	// Evaluate the head

	auto head = _head;

	while (true) {
		auto new_head = head->evaluate(head, evaluation);
		if (new_head) {
			head = new_head;
		} else {
			break;
		}
	}

	// Evaluate the leaves from left to right (according to Hold values).
	BaseExpressionRef result;

	if (head->type() != SymbolType) {
		if (head != _head) {
			ExpressionRef temp = boost::static_pointer_cast<const Expression>(clone(head));
			result = temp->evaluate_from_expression_head(temp, evaluation);
		} else {
			result = evaluate_from_expression_head(boost::static_pointer_cast<const Expression>(self), evaluation);
		}
	} else {
		const auto head_symbol = static_cast<const Symbol *>(head.get());

		return head_symbol->evaluate_with_head()(
			boost::static_pointer_cast<const Expression>(self),
			head,
			_slice_id,
			_slice_ptr,
			evaluation);
	}

	if (result) {
		return result;
	} else if (head != _head) {
		return clone(head);
	} else {
		return BaseExpressionRef();
	}
}
