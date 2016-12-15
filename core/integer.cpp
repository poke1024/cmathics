#include "types.h"
#include "expression.h"
#include "integer.h"
#include "evaluation.h"

BaseExpressionPtr MachineInteger::head(const Evaluation &evaluation) const {
    return evaluation.Integer;
}

BaseExpressionPtr BigInteger::head(const Evaluation &evaluation) const {
    return evaluation.Integer;
}
