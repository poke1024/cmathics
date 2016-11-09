#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "pattern.h"
#include "expression.h"
#include "definitions.h"
#include "integer.h"
#include "string.h"
#include "real.h"

void MatchContext::add_matched_variable(const Symbol *variable) {
    variable->link_variable(_matched_variables_head);
    _matched_variables_head = variable;
}
