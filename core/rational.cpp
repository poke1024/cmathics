#include "types.h"
#include "expression.h"
#include "rational.h"

BaseExpressionPtr BigRational::head(const Evaluation &evaluation) const {
    return evaluation.Rational;
}
