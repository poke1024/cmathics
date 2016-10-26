#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "pattern.h"
#include "expression.h"
#include "definitions.h"
#include "integer.h"
#include "string.h"
#include "real.h"

bool Matcher::heads(size_t n, const Symbol *head) const {
    const BaseExpressionPtr head_expr =
        static_cast<BaseExpressionPtr>(head);

    for (size_t i = 0; i < n; i++) {
        if (head_expr != _sequence[i]->get_head_ptr()) {
            return i;
        }
    }

    return n;
}

Match Matcher::consume(size_t n) const {
    assert(_sequence.size() >= n);

    if (_next_pattern.size() == 0) {
        if (_sequence.size() == n) {
            return Match(true);
        } else {
            return Match(false);
        }
    } else {
        auto next = _next_pattern[0];
        auto rest = _next_pattern.slice(1);
        return next->match_sequence(Matcher(_definitions, next, rest, _sequence.slice(n)));
    }
}

Match Matcher::blank_sequence(match_size_t k, const Symbol *head) const {
    const size_t n = _sequence.size();

    match_size_t min_remaining = n;
    match_size_t max_remaining = n;

    for (auto patt : _next_pattern) {
        size_t min_args, max_args;
        std::tie(min_args, max_args) = patt->match_num_args();

        if ((max_remaining -= min_args) < k) {
            return Match(false);
        }
        if ((min_remaining -= max_args) < 0) {
            min_remaining = 0;
        }
    }

    min_remaining = std::max(min_remaining, k);

    if (head) {
        max_remaining = std::min(
            max_remaining, (match_size_t)heads(max_remaining, head));
    }

    for (size_t l = max_remaining; l >= min_remaining; l--) {
        auto match = (*this)(l, nullptr);
        if (match) {
            return match;
        }
    }

    return Match(false);
}

Match Matcher::operator()(size_t match_size, const Symbol *head) const {
    if (_sequence.size() < match_size) {
        return Match(false);
    }

    if (head && heads(match_size, head) != match_size) {
        return Match(false);
    }

    if (!_variable) {
        return consume(match_size);
    }

    BaseExpressionRef item;

    if (match_size == 1) {
        item = _sequence[0];
    } else {
        item = std::make_shared<Expression>(
            _definitions->sequence(), _sequence.slice(0, match_size));
    }

    const BaseExpressionRef &existing = _variable->matched_value();
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

Match Matcher::operator()(const Symbol *var, const BaseExpressionRef &pattern) const {
    auto matcher = Matcher(_definitions, var, pattern, _next_pattern, _sequence);
    return pattern->match_sequence(matcher);
}

const Symbol *blank_head(const Expression *patt) {
    switch (patt->_leaves.size()) {
        case 0:
            return nullptr;

        case 1: {
            auto symbol = patt->leaf_ptr(0);
            if (symbol->type() == SymbolType) {
                return static_cast<const Symbol *>(symbol);
            }
        }
        // fallthrough

        default:
            return nullptr;
    }
}
