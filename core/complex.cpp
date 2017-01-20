#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "types.h"
#include "complex.h"
#include "expression.h"

const std::hash<machine_real_t> MachineComplex::hash_function = std::hash<machine_real_t>();

BaseExpressionPtr MachineComplex::head(const Evaluation &evaluation) const {
    return evaluation.Complex;
}

BaseExpressionPtr BigComplex::head(const Evaluation &evaluation) const {
    return evaluation.Complex;
}
