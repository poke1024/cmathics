#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include "types.h"
#include "hash.h"
#include "integer.h"
#include "real.h"
#include "rational.h"
#include "string.h"

const hash_t symbol_hash = djb2("Symbol");
const hash_t machine_integer_hash = djb2("MachineInteger");
const hash_t string_hash = djb2("String");
const hash_t machine_real_hash = djb2("MachineReal");
const hash_t rational_hash = djb2("Rational");
