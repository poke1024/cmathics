#ifndef CMATHICS_MATCHER_H
#define CMATHICS_MATCHER_H

#include <assert.h>

#include "types.h"
#include "evaluation.h"
#include "symbol.h"

class RefsSlice;

class Matcher {
private:
	MatchContext &_context;
	const Symbol *_variable;
	const BaseExpressionRef &_this_pattern;
	const RefsSlice &_next_pattern;
	const RefsSlice &_sequence;

	bool heads(size_t n, const Symbol *head) const;
	bool consume(size_t n) const;
	bool match_variable(size_t match_size, const BaseExpressionRef &item) const;

public:
	inline Matcher(MatchContext &context, const BaseExpressionRef &this_pattern, const RefsSlice &next_pattern, const RefsSlice &sequence) :
			_context(context), _variable(nullptr), _this_pattern(this_pattern), _next_pattern(next_pattern), _sequence(sequence) {
	}

	inline Matcher(MatchContext &context, const Symbol *variable, const BaseExpressionRef &this_pattern, const RefsSlice &next_pattern, const RefsSlice &sequence) :
			_context(context), _variable(variable), _this_pattern(this_pattern), _next_pattern(next_pattern), _sequence(sequence) {
	}

	inline const BaseExpressionRef &this_pattern() const {
		return _this_pattern;
	}

	inline const RefsSlice &sequence() const {
		return _sequence;
	}

	bool operator()(size_t match_size, const Symbol *head) const;

	bool operator()(const Symbol *var, const BaseExpressionRef &pattern) const;

	bool descend() const;

	bool blank_sequence(match_size_t k, const Symbol *head) const;
};

#include "leaves.h"

inline Match match(const BaseExpressionRef &patt, const BaseExpressionRef &item, Definitions &definitions) {
	MatchContext context(patt, item, definitions);
	{
		Matcher matcher(context, patt, empty_slice, RefsSlice(item));

		if (patt->match_sequence(matcher)) {
			return Match(true, context);
		} else {
			return Match(); // no match
		}
	}
}

#endif //CMATHICS_MATCHER_H
