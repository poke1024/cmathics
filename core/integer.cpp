#include "types.h"
#include "expression.h"
#include "integer.h"
#include "evaluation.h"

BaseExpressionPtr MachineInteger::head(const Evaluation &evaluation) const {
    return evaluation.Integer;
}

std::string MachineInteger::format(const SymbolRef &form, const Evaluation &evaluation) const {
    return std::to_string(value);
}

BaseExpressionPtr BigInteger::head(const Evaluation &evaluation) const {
    return evaluation.Integer;
}

std::string BigInteger::format(const SymbolRef &form, const Evaluation &evaluation) const {
    return value.get_str();
}
