#include "types.h"
#include "expression.h"
#include "integer.h"
#include "evaluation.h"

std::string MachineInteger::debugform() const {
	return std::to_string(value);
}

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

BaseExpressionRef MachineInteger::negate(const Evaluation &evaluation) const {
    return from_primitive(Numeric::Z(value) * Numeric::Z(-1));
}

std::string BigInteger::debugform() const {
	return value.get_str();
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
	return value.get_str();
}

BaseExpressionRef BigInteger::negate(const Evaluation &evaluation) const {
    mpz_class negated(-value);
    return from_primitive(std::move(negated));
}
