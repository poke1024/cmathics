#include "types.h"
#include "expression.h"
#include "integer.h"
#include "evaluation.h"

BaseExpressionRef MachineInteger::make_boxes(
    BaseExpressionPtr form,
    const Evaluation &evaluation) const {

    return Pool::String(std::to_string(value));
}

std::string MachineInteger::boxes_to_text(const Evaluation &evaluation) const {
	return std::to_string(value);
}

BaseExpressionPtr MachineInteger::head(const Symbols &symbols) const {
    return symbols.Integer;
}

BaseExpressionRef BigInteger::make_boxes(
    BaseExpressionPtr form,
    const Evaluation &evaluation) const {

    return Pool::String(value.get_str());
}

BaseExpressionPtr BigInteger::head(const Symbols &symbols) const {
    return symbols.Integer;
}

std::string BigInteger::boxes_to_text(const Evaluation &evaluation) const {
	return value.get_str();;
}
