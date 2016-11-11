#ifndef CMATHICS_MATCHER_H
#define CMATHICS_MATCHER_H

#include <assert.h>

#include "types.h"
#include "evaluation.h"
#include "symbol.h"


class MatchContext {
private:
	Symbol *_matched_variables_head;

public:
	const MatchId id;
	Definitions &definitions;

	inline MatchContext(const BaseExpressionRef &patt, const BaseExpressionRef &item, Definitions &defs) :
			id(patt, item), definitions(defs), _matched_variables_head(nullptr) {
	}

	inline void add_matched_variable(Symbol *variable) {
		variable->set_next_variable(_matched_variables_head);
		_matched_variables_head = variable;
	}

	inline void prepend_matched_variables(Symbol *variable) {
		if (variable) {
			Symbol *appended = _matched_variables_head;
			_matched_variables_head = variable;
			while (true) {
				Symbol *next = variable->next_variable();
				if (!next) {
					break;
				}
				variable = next;
			}
			variable->set_next_variable(appended);
		}
	}

	inline Symbol *matched_variables() const {
		return _matched_variables_head;
	}

	inline Symbol *remove_matched_variables() {
		Symbol *head = _matched_variables_head;
		_matched_variables_head = nullptr;
		return head;
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
			_matched(matched), _id(context.id), _variables(context.matched_variables()) {
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

class RefsSlice;

inline const Symbol *blank_head(RefsExpressionPtr patt) {
	switch (patt->_leaves.size()) {
		case 0:
			return nullptr;

		case 1: {
			auto symbol = patt->leaf(0);
			if (symbol->type() == SymbolType) {
				// ok to get Symbol's address here as "symbol" gets out of
				// scope, as we got this symbol from a RefsExpression that
				// keeps on referencing this Symbol.
				return static_cast<const Symbol *>(symbol.get());
			}
		}
			// fallthrough

		default:
			return nullptr;
	}
}

template<typename Slice>
class Matcher {
private:
	MatchContext &_context;
	Symbol *_variable;
	const BaseExpressionRef &_this_pattern;
	const RefsSlice &_next_pattern;
	const Slice &_sequence;

	bool heads(size_t n, const Symbol *head) const;

	bool consume(size_t n) const;

	bool match_variable(size_t match_size, const BaseExpressionRef &item) const;

public:
	inline Matcher(
		MatchContext &context,
		const BaseExpressionRef &this_pattern,
		const RefsSlice &next_pattern,
		const Slice &sequence) :
		_context(context),
		_variable(nullptr),
		_this_pattern(this_pattern),
		_next_pattern(next_pattern),
		_sequence(sequence) {
	}

	inline Matcher(
		MatchContext &context,
		Symbol *variable,
		const BaseExpressionRef &this_pattern,
		const RefsSlice &next_pattern,
		const Slice &sequence) :
		_context(context),
		_variable(variable),
		_this_pattern(this_pattern),
		_next_pattern(next_pattern),
		_sequence(sequence) {
	}

	bool shift(size_t match_size, const Symbol *head, bool make_sequence=false) const;

	bool shift(Symbol *var, const BaseExpressionRef &pattern) const;

	bool blank_sequence(match_size_t k, const Symbol *head) const;

	bool match_sequence() const {
		switch (_this_pattern->type()) {
			case ExpressionType: {
				auto patt_expr = _this_pattern->to_refs_expression(_this_pattern);
				return match_sequence_with_head(patt_expr.get());
			}

			default:
				// expr is always pattern[0].
				if ( _sequence.size() == 0) {
					return false;
				} else if (_this_pattern->same(_sequence[0])) {
					return shift(1, nullptr);
				} else {
					return false;
				}
		}
	}

	bool match_sequence_with_head(RefsExpressionPtr patt) const {
		const auto patt_head = patt->_head;

		switch (patt_head->type()) {
			case SymbolType:
				switch (boost::static_pointer_cast<const Symbol>(patt_head)->_id) {
					case SymbolId::Blank:
						return shift(1, blank_head(patt));
					case SymbolId::BlankSequence:
						return blank_sequence(1, blank_head(patt));
					case SymbolId::BlankNullSequence:
						return blank_sequence(0, blank_head(patt));
					case SymbolId::Pattern: {
						auto var_expr = patt->_leaves[0].get();
						if (var_expr->type() == SymbolType) {
							Symbol *var = const_cast<Symbol*>(static_cast<const Symbol*>(var_expr));
							return shift(var, patt->_leaves[1]);
						}
					}
					default: {
						// nothing
					}
				}
				// fallthrough

			default:
				if (_sequence.size() == 0) {
					return false;
				} else {
					auto next = _sequence[0];
					if (next->type() == ExpressionType) {
						auto next_expr = boost::static_pointer_cast<const Expression>(next);

						Symbol *head_match = nullptr;

						if (patt_head->type() == ExpressionType &&
						    next_expr->head()->type() == ExpressionType) {

							assert(_context.matched_variables() == nullptr);
							if (next_expr->head()->match_leaves(_context, patt_head)) {
								head_match = _context.remove_matched_variables();
							} else {
								return false;
							}
						}

						if (!next->match_leaves(_context, _this_pattern)) {
							return false;
						}

						if (!consume(1)) {
							return false;
						}

						_context.prepend_matched_variables(head_match);
						return true;
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
		Matcher<InPlaceRefsSlice<1>> matcher(
			context, patt, RefsSlice(), InPlaceRefsSlice<1>(&item, 1, item->type_mask()));

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
		if (head_expr != _sequence[i]->head_ptr()) {
			return i;
		}
	}

	return n;
}

template<typename Slice>
bool Matcher<Slice>::consume(size_t n) const {
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

		Matcher<Slice> matcher(_context, next, rest, _sequence.slice(n));
		return matcher.match_sequence();
	}
}

template<typename Slice>
bool Matcher<Slice>::blank_sequence(match_size_t k, const Symbol *head) const {
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
			_context.add_matched_variable(_variable);
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
		return match_variable(match_size, _sequence[0]);
	} else {
		return match_variable(match_size, expression(
			_context.definitions.Sequence(),
			_sequence.slice(0, match_size)));
	}
}

template<typename Slice>
bool Matcher<Slice>::shift(Symbol *var, const BaseExpressionRef &pattern) const {
	Matcher<Slice> matcher(_context, var, pattern, _next_pattern, _sequence);
	return matcher.match_sequence();
}

template<typename Slice>
bool ExpressionImplementation<Slice>::match_leaves(MatchContext &context, const BaseExpressionRef &patt) const {
	auto patt_expr = patt->to_refs_expression(patt);
	auto patt_sequence = patt_expr->_leaves;

	Matcher <Slice> matcher(context, patt_sequence[0], patt_sequence.slice(1), _leaves);
	return matcher.match_sequence();
}

#endif //CMATHICS_MATCHER_H
