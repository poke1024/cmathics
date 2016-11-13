#ifndef CMATHICS_MATCHER_H
#define CMATHICS_MATCHER_H

#include <assert.h>

#include "types.h"
#include "evaluation.h"
#include "symbol.h"

class VariableList {
private:
    Symbol *_first;
    Symbol *_last;

public:
    inline VariableList() : _first(nullptr), _last(nullptr) {
    }

    inline VariableList(const VariableList &list) : _first(list._first), _last(list._last) {
    }

    inline bool is_empty() const {
        return _first == nullptr;
    }

    inline void reset() {
        _first = nullptr;
        _last = nullptr;
    }

    inline Symbol *get() const {
        return _first;
    }

    inline void prepend(Symbol *symbol) {
        Symbol *old_first = _first;
        if (old_first) {
            symbol->set_next_variable(old_first);
            _first = symbol;
        } else {
            symbol->set_next_variable(nullptr);
            _first = symbol;
            _last = symbol;
        }
    }

    inline void prepend(const VariableList &list) {
        Symbol *list_first = list._first;
        if (list_first) {
            list._last->set_next_variable(_first);
            _first = list_first;
        }
    }
};

class MatchContext {
public:
	const MatchId id;
	Definitions &definitions;
    VariableList matched_variables;

	inline MatchContext(const BaseExpressionRef &patt, const BaseExpressionRef &item, Definitions &defs) :
        id(patt, item), definitions(defs) {
	}
};

class Match {
private:
	const bool _matched;
	const MatchId _id;
	const Symbol *_variables;

public:
	explicit inline Match() : _matched(false) {
	}

	explicit inline Match(bool matched, const MatchContext &context) :
		_matched(matched), _id(context.id), _variables(context.matched_variables.get()) {
	}

	inline operator bool() const {
		return _matched;
	}

	inline const MatchId &id() const {
		return _id;
	}

	inline const Symbol *variables() const {
		return _variables;
	}

	template<int N>
	typename BaseExpressionTuple<N>::type get() const;
};

template<int M, int N>
struct unpack_symbols {
	void operator()(const Symbol *symbol, typename BaseExpressionTuple<M>::type &t) {
		// symbols are already ordered in the order of their (first) appearance in the original pattern.
		assert(symbol != nullptr);
		std::get<N>(t) = symbol->matched_value();
		unpack_symbols<M, N + 1>()(symbol->next_variable(), t);
	}
};

template<int M>
struct unpack_symbols<M, M> {
	void operator()(const Symbol *symbol, typename BaseExpressionTuple<M>::type &t) {
		assert(symbol == nullptr);
	}
};

template<int N>
inline typename BaseExpressionTuple<N>::type Match::get() const {
	typename BaseExpressionTuple<N>::type t;
	unpack_symbols<N, 0>()(_variables, t);
	return t;
};

class DynamicSlice;

inline const Symbol *blank_head(const BaseExpressionRef *begin, const BaseExpressionRef *end) {
	if (end - begin == 1) { // exactly one leaf?
		const BaseExpressionRef &symbol = *begin;
		if (symbol->type() == SymbolType) {
			// ok to get Symbol's address here as "symbol" gets out of
			// scope, as we got this symbol from a begin pointer that
			// keeps on referencing this Symbol.
			return static_cast<const Symbol *>(symbol.get());
		}
	}

	return nullptr;
}

template<typename Slice>
class Matcher {
private:
	MatchContext &_context;
	Symbol *_variable;

	const BaseExpressionRef *_curr_pattern;
	const BaseExpressionRef *_rest_pattern_begin;
	const BaseExpressionRef *_rest_pattern_end;

	const Slice &_sequence;
	const size_t _sequence_offset;

	bool heads(size_t n, const Symbol *head) const;

	bool consume(size_t n) const;

	bool match_variable(size_t match_size, const BaseExpressionRef &item) const;

public:
	inline Matcher(
		MatchContext &context,
		const BaseExpressionRef *curr_pattern,
		const BaseExpressionRef *rest_pattern_begin,
		const BaseExpressionRef *rest_pattern_end,
		const Slice &sequence,
		size_t sequence_offset) :
		_context(context),
		_variable(nullptr),
		_curr_pattern(curr_pattern),
		_rest_pattern_begin(rest_pattern_begin),
		_rest_pattern_end(rest_pattern_end),
		_sequence(sequence),
		_sequence_offset(sequence_offset) {
	}

	inline Matcher(
		MatchContext &context,
		Symbol *variable,
		const BaseExpressionRef *curr_pattern,
		const BaseExpressionRef *rest_pattern_begin,
		const BaseExpressionRef *rest_pattern_end,
		const Slice &sequence,
		size_t sequence_offset) :
		_context(context),
		_variable(variable),
		_curr_pattern(curr_pattern),
		_rest_pattern_begin(rest_pattern_begin),
		_rest_pattern_end(rest_pattern_end),
		_sequence(sequence),
		_sequence_offset(sequence_offset) {
	}

	bool shift(size_t match_size, const Symbol *head, bool make_sequence=false) const;

	bool shift(Symbol *var, const BaseExpressionRef *curr_pattern) const;

	bool blank_sequence(match_size_t k, const Symbol *head) const;

	bool match_sequence() const {
		const BaseExpressionRef &patt = *_curr_pattern;

		switch (patt->type()) {
			case ExpressionType: {
				const Expression *patt_expr = static_cast<const Expression*>(patt.get());
				const Matcher *matcher = this;

				return patt_expr->with_leaves_array([matcher, patt_expr] (const BaseExpressionRef *patt_leaves, size_t size) {
					return matcher->match_sequence_with_head(patt_expr->_head, patt_leaves, patt_leaves + size);
				});
			}

			default:
				// expr is always pattern[0].
				if ( _sequence.size() <= _sequence_offset) {
					return false;
				} else if (patt->same(_sequence[_sequence_offset])) {
					return shift(1, nullptr);
				} else {
					return false;
				}
		}
	}

	bool match_sequence_with_head(
		const BaseExpressionRef &patt_head,
		const BaseExpressionRef *patt_begin,
	    const BaseExpressionRef *patt_end) const {

		switch (patt_head->extended_type()) {
			case SymbolBlank:
				return shift(1, blank_head(patt_begin, patt_end));

			case SymbolBlankSequence:
				return blank_sequence(1, blank_head(patt_begin, patt_end));

			case SymbolBlankNullSequence:
				return blank_sequence(0, blank_head(patt_begin, patt_end));

			case SymbolPattern: {
				if (patt_end - patt_begin == 2) { // 2 leaves?
					const BaseExpressionRef &leaf_1 = *patt_begin;
					if (leaf_1->type() == SymbolType) {
						Symbol *var = const_cast<Symbol*>(static_cast<const Symbol*>(leaf_1.get()));
						const BaseExpressionRef *leaf_2 = patt_begin + 1;
						return shift(var, leaf_2);
					}
				}
			}
			// fallthrough

			default:
				if (_sequence.size() <= _sequence_offset) {
					return false;
				} else {
					auto next = _sequence[_sequence_offset];
					if (next->type() == ExpressionType) {
						auto next_expr = boost::static_pointer_cast<const Expression>(next);

						Symbol *head_match = nullptr;

						if (patt_head->type() == ExpressionType &&
						    next_expr->head()->type() == ExpressionType) {

							assert(_context.matched_variables.is_empty());

							if (!next_expr->head()->match_leaves(_context, patt_head)) {
                                return false;
							}

                            VariableList matched_variables(_context.matched_variables);
                            _context.matched_variables.reset();

                            if (!next->match_leaves(_context, *_curr_pattern)) {
                                return false;
                            }

                            if (!consume(1)) {
                                return false;
                            }

                            _context.matched_variables.prepend(matched_variables);
                            return true;
						} else {
                            if (!next->match_leaves(_context, *_curr_pattern)) {
                                return false;
                            }

                            if (!consume(1)) {
                                return false;
                            }

                            return true;
                        }
                    } else {
						return false;
					}
				}
		}
	}
};

#include "leaves.h"

inline Match match(const BaseExpressionRef &patt, const BaseExpressionRef &item, Definitions &definitions) {
	MatchContext context(patt, item, definitions);
	{
		Matcher<StaticSlice<1>> matcher(
			context, &patt, nullptr, nullptr, StaticSlice<1>(&item, item->type_mask()), 0);

		if (matcher.match_sequence()) {
			return Match(true, context);
		} else {
			return Match(); // no match
		}
	}
}


template<typename Slice>
bool Matcher<Slice>::heads(size_t n, const Symbol *head) const {
	const BaseExpressionPtr head_expr =
		static_cast<BaseExpressionPtr>(head);

	for (size_t i = 0; i < n; i++) {
		if (head_expr != _sequence[_sequence_offset + i]->head_ptr()) {
			return i;
		}
	}

	return n;
}

template<typename Slice>
bool Matcher<Slice>::consume(size_t n) const {
	assert(_sequence.size() >= _sequence_offset + n);

	if (_rest_pattern_end == _rest_pattern_begin) {
		if (_sequence.size() == _sequence_offset + n) {
			return true;
		} else {
			return false;
		}
	} else {
		Matcher<Slice> matcher(
			_context,
			_rest_pattern_begin,
			_rest_pattern_begin + 1,
			_rest_pattern_end,
			_sequence,
			_sequence_offset + n);

		return matcher.match_sequence();
	}
}

template<typename Slice>
bool Matcher<Slice>::blank_sequence(match_size_t k, const Symbol *head) const {
	const size_t n = _sequence.size();

	match_size_t min_remaining = n;
	match_size_t max_remaining = n;

	for (const BaseExpressionRef *it = _rest_pattern_begin; it < _rest_pattern_end; it++) {
		const BaseExpressionRef &patt = *it;

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
		max_remaining = std::min(max_remaining, (match_size_t)heads(max_remaining, head));
	}

	for (size_t l = max_remaining; l >= min_remaining; l--) {
		auto match = shift(l, nullptr, true);
		if (match) {
			return match;
		}
	}

	return false;
}

template<typename Slice>
bool Matcher<Slice>::match_variable(size_t match_size, const BaseExpressionRef &item) const {
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
			_context.matched_variables.prepend(_variable);
		} else {
			_variable->clear_matched_value();
		}

		return match;
	}
}

template<typename Slice>
bool Matcher<Slice>::shift(size_t match_size, const Symbol *head, bool make_sequence) const {
	if (_sequence.size() < match_size) {
		return false;
	}

	if (head && heads(match_size, head) != match_size) {
		return false;
	}

	if (!_variable) {
		return consume(match_size);
	}

	if (match_size == 1 && !make_sequence) {
		return match_variable(match_size, _sequence[_sequence_offset]);
	} else {
		return match_variable(match_size, expression(
			_context.definitions.Sequence(),
			_sequence)->slice(0, match_size));
	}
}

template<typename Slice>
bool Matcher<Slice>::shift(Symbol *var, const BaseExpressionRef *curr_pattern) const {
	Matcher<Slice> matcher(
		_context, var, curr_pattern, _rest_pattern_begin, _rest_pattern_end, _sequence, _sequence_offset);
	return matcher.match_sequence();
}

template<typename Slice>
bool ExpressionImplementation<Slice>::match_leaves(MatchContext &context, const BaseExpressionRef &patt) const {
	if (patt->type() != ExpressionType) {
		return false;
	}

	const Expression *patt_expr = static_cast<const Expression*>(patt.get());
	const Slice &leaves = _leaves;

	return patt_expr->with_leaves_array([&context, &leaves] (const BaseExpressionRef *patt_leaves, size_t size) {
		Matcher<Slice> matcher(context, patt_leaves, patt_leaves + 1, patt_leaves + size, leaves, 0);
		return matcher.match_sequence();
	});
}

#endif //CMATHICS_MATCHER_H
