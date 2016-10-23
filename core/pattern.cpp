#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "pattern.h"
#include "expression.h"
#include "definitions.h"
#include "integer.h"
#include "string.h"
#include "real.h"

Match Matcher::consume(size_t n) const {
    if (_next_pattern.size() == 0) {
        if (_sequence.size() == n) {
            return Match(true);
        } else {
            return Match(false);
        }
    } else {
        assert(_sequence.size() >= n);
        auto next = _next_pattern[0];
        auto rest = _next_pattern.slice(1);
        return next->match_sequence(Matcher(_definitions, next, rest, _sequence.slice(n)));
    }
}

match_size_t Matcher::remaining(match_size_t min_remaining) const {
    match_size_t remaining = _sequence.size();
    for (auto patt : _next_pattern) {
        size_t min_args, max_args;
        std::tie(min_args, max_args) = patt->match_num_args();
        if ((remaining -= min_args) < min_remaining) {
            return remaining;
        }
    }
    return remaining;
}

Match Matcher::operator()(size_t match_size) const {
    if (_sequence.size() < match_size) {
        return Match(false);
    }

    if (!_variable) {
        return consume(match_size);
    }

    BaseExpressionPtr item;

    if (match_size == 1) {
        item = _sequence[0];
    } else {
        item = new Expression(_definitions->sequence(), _sequence.slice(0, match_size)); // FIXME
    }

    const BaseExpression *existing = _variable->matched_value();
    if (existing) {
        if (existing->same(item)) {
            return consume(match_size);
        } else {
            return Match(false);
        }
    } else {
        Match match;

        _variable->set_matched_value(item);
        try {
            match = consume(match_size);
        } catch(...) {
            _variable->clear_matched_value();
            throw;
        }
        _variable->clear_matched_value();

        if (match) {
            match.add_variable(_variable, item); // add leaves only
        }

        return match;
    }
}

Match Matcher::operator()(const Symbol *var, BaseExpressionPtr pattern) const {
    auto matcher = Matcher(_definitions, var, pattern, _next_pattern, _sequence);
    return pattern->match_sequence(matcher);
}

