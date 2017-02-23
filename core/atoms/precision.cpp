#include "../types.h"
#include "core/expression/implementation.h"
#include "precision.h"

constexpr mp_prec_t machine_precision_bits = mp_prec_t(std::numeric_limits<machine_real_t>::digits);

const Precision Precision::none(0L);
const Precision Precision::machine_precision(machine_precision_bits);
