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

bool Matcher::heads(size_t n, const Symbol *head) const {
    const BaseExpressionPtr head_expr =
        static_cast<BaseExpressionPtr>(head);

    for (size_t i = 0; i < n; i++) {
        if (head_expr != _sequence[i]->head_ptr()) {
            return i;
        }
    }

    return n;
}

bool Matcher::consume(size_t n) const {
    assert(_sequence.size() >= n);

    if (_next_pattern.size() == 0) {
        if (_sequence.size() == n) {
            return true;
        } else {
            return false;
        }
    } else {
        auto next = _next_pattern[0];
        auto rest = _next_pattern.slice(1);

        return next->match_sequence(Matcher(_context, next, rest, _sequence.slice(n)));
    }
}

bool Matcher::blank_sequence(match_size_t k, const Symbol *head) const {
    const size_t n = _sequence.size();

    match_size_t min_remaining = n;
    match_size_t max_remaining = n;

    for (auto patt : _next_pattern) {
        size_t min_args, max_args;
        std::tie(min_args, max_args) = patt->match_num_args();

        if ((max_remaining -= min_args) < k) {
            return false;
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

    return false;
}

bool Matcher::match_variable(size_t match_size, const BaseExpressionRef &item) const {
    const auto match_id = _context.id;
    BaseExpressionRef existing = _variable->matched_value(match_id);
    if (existing) {
        if (existing->same(item)) {
            return consume(match_size);
        } else {
            return false;
        }
    } else {
        bool match;

        _variable->set_matched_value(match_id, item);
        try {
            match = consume(match_size);
        } catch(...) {
            _variable->clear_matched_value();
            throw;
        }

        if (match) {
            _context.add_matched_variable(_variable);
        } else {
            _variable->clear_matched_value();
        }

        return match;
    }
}

bool Matcher::operator()(size_t match_size, const Symbol *head) const {
    if (_sequence.size() < match_size) {
        return false;
    }

    if (head && heads(match_size, head) != match_size) {
        return false;
    }

    if (!_variable) {
        return consume(match_size);
    }

    if (match_size == 1) {
        return match_variable(match_size, _sequence[0]);
    } else {
        return match_variable(match_size, expression(
            _context.definitions.sequence(),
            _sequence.slice(0, match_size)));
    }
}

bool Matcher::operator()(const Symbol *var, const BaseExpressionRef &pattern) const {
    return pattern->match_sequence(Matcher(_context, var, pattern, _next_pattern, _sequence));
}

bool Matcher::descend() const {
    assert(_this_pattern->type() == ExpressionType);
    auto patt_expr = std::static_pointer_cast<const Expression>(_this_pattern);
    auto patt_sequence = patt_expr->leaves();
    auto patt_next = patt_sequence[0];

    assert(_sequence[0]->type() == ExpressionType);
    auto item_expr = std::static_pointer_cast<const Expression>(_sequence[0]);
    auto item_sequence = item_expr->leaves();

    const auto matcher = Matcher(_context, patt_next, patt_sequence.slice(1), item_sequence);
    if (patt_next->match_sequence(matcher)) {
        return consume(1); // if leaves matched, consume whole expression now
    } else {
        return false;
    }
}


const Symbol *blank_head(ExpressionPtr patt) {
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
