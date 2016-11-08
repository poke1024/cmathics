#ifndef PATTERN_H
#define PATTERN_H

#include <assert.h>

#include "types.h"
#include "definitions.h"
#include "evaluation.h"
#include "matcher.h"

#include <iostream>

const Symbol *blank_head(RefsExpressionPtr patt);

class Blank : public Symbol {
public:
    Blank(Definitions *definitions) :
        Symbol(definitions, "System`Blank") {
    }

    virtual match_sizes_t match_num_args_with_head(const RefsExpression *patt) const {
        return std::make_tuple(1, 1);
    }

    virtual bool match_sequence_with_head(const RefsExpression *patt, const Matcher &matcher) const {
        return matcher(1, blank_head(patt));
    }
};

template<int MINIMUM>
class BlankSequenceBase : public Symbol {
public:
    BlankSequenceBase(Definitions *definitions, const char *name) :
        Symbol(definitions, name) {
    }

    virtual match_sizes_t match_num_args_with_head(const RefsExpression *patt) const {
        return std::make_tuple(MINIMUM, MATCH_MAX);
    }

    virtual bool match_sequence_with_head(const RefsExpression *patt, const Matcher &matcher) const {
        return matcher.blank_sequence(MINIMUM, blank_head(patt));
    }
};

class BlankSequence : public BlankSequenceBase<1> {
public:
    BlankSequence(Definitions *definitions) :
        BlankSequenceBase(definitions, "System`BlankSequence") {
    }
};

class BlankNullSequence : public BlankSequenceBase<0> {
public:
    BlankNullSequence(Definitions *definitions) :
        BlankSequenceBase(definitions, "System`BlankNullSequence") {
    }
};

class Pattern : public Symbol {
public:
    Pattern(Definitions *definitions) :
        Symbol(definitions, "System`Pattern") {
    }

    virtual bool match_sequence_with_head(RefsExpressionPtr patt, const Matcher &matcher) const {
        auto var_expr = patt->_leaves[0].get();
        assert(var_expr->type() == SymbolType);
        // if nullptr -> throw self.error('patvar', expr)
        const Symbol *var = static_cast<const Symbol*>(var_expr);
        return matcher(var, patt->_leaves[1]);
    }

    virtual match_sizes_t match_num_args_with_head(RefsExpressionPtr patt) const {
        if (patt->_leaves.size() == 2) {
            // Pattern is only valid with two arguments
            return patt->_leaves[1]->match_num_args();
        } else {
            return std::make_tuple(1, 1);
        }
    }
};

class Alternatives : public Symbol {
public:
    Alternatives(Definitions *definitions) :
        Symbol(definitions, "System`Alternatives") {
    }

    virtual match_sizes_t match_num_args_with_head(RefsExpressionPtr patt) const {
        auto leaves = patt->_leaves;

        if (leaves.size() == 0) {
            return std::make_tuple(1, 1);
        }

        size_t min_p, max_p;
        std::tie(min_p, max_p) = leaves[0]->match_num_args();

        for (auto i = std::begin(leaves) + 1; i != std::end(leaves); i++) {
            size_t min_leaf, max_leaf;
            std::tie(min_leaf, max_leaf) = (*i)->match_num_args();
            max_p = std::max(max_p, max_leaf);
            min_p = std::min(min_p, min_leaf);
        }

        return std::make_tuple(min_p, max_p);
    }
};

class Repeated : public Symbol {
public:
    Repeated(Definitions *definitions) :
        Symbol(definitions, "System`Repeated") {
    }

    virtual match_sizes_t match_num_args_with_head(RefsExpressionPtr patt) const {
        switch(patt->_leaves.size()) {
            case 1:
                return std::make_tuple(1, MATCH_MAX);
            case 2:
                // TODO inspect second arg
                return std::make_tuple(1, MATCH_MAX);
            default:
                return std::make_tuple(1, 1);
        }
    }
};

template<typename Slice>
bool MatcherImplementation<Slice>::heads(size_t n, const Symbol *head) const {
	const BaseExpressionPtr head_expr =
			static_cast<BaseExpressionPtr>(head);

	for (size_t i = 0; i < n; i++) {
		if (head_expr != _sequence[i]->head_ptr()) {
			return i;
		}
	}

	return n;
}

template<typename Slice>
bool MatcherImplementation<Slice>::consume(size_t n) const {
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

		return next->match_sequence(
			MatcherImplementation<Slice>(_context, next, rest, _sequence.slice(n)));
	}
}

template<typename Slice>
bool MatcherImplementation<Slice>::blank_sequence(match_size_t k, const Symbol *head) const {
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

template<typename Slice>
bool MatcherImplementation<Slice>::match_variable(size_t match_size, const BaseExpressionRef &item) const {
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

template<typename Slice>
bool MatcherImplementation<Slice>::operator()(size_t match_size, const Symbol *head) const {
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

template<typename Slice>
bool MatcherImplementation<Slice>::operator()(const Symbol *var, const BaseExpressionRef &pattern) const {
	return pattern->match_sequence(MatcherImplementation<Slice>(_context, var, pattern, _next_pattern, _sequence));
}

template<typename Slice>
bool MatcherImplementation<Slice>::descend() const {
	assert(_this_pattern->type() == ExpressionType);
	auto patt_expr = _this_pattern->to_refs_expression(_this_pattern);
	auto patt_sequence = patt_expr->_leaves;
	auto patt_next = patt_sequence[0];

	if (_sequence[0]->descend_match(_context, patt_next, patt_sequence.slice(1))) {
		return consume(1); // if leaves matched, consume whole expression now
	} else {
		return false;
	}
}

#endif
