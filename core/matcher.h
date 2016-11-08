#ifndef CMATHICS_MATCHER_H
#define CMATHICS_MATCHER_H

#include <assert.h>

#include "types.h"
#include "evaluation.h"
#include "symbol.h"

class RefsSlice;

class Matcher {
protected:
	const BaseExpressionRef &_this_pattern;

public:
	inline Matcher(const BaseExpressionRef &this_pattern) : _this_pattern(this_pattern) {
	}

	inline const BaseExpressionRef &this_pattern() const {
		return _this_pattern;
	}

	virtual size_t sequence_size() const = 0;

	virtual BaseExpressionRef sequence_next() const = 0;

	virtual bool operator()(size_t match_size, const Symbol *head) const = 0;

	virtual bool operator()(const Symbol *var, const BaseExpressionRef &pattern) const = 0;

	virtual bool descend() const = 0;

	virtual bool blank_sequence(match_size_t k, const Symbol *head) const = 0;
};

template<typename Slice>
class MatcherImplementation : public Matcher {
private:
	MatchContext &_context;
	const Symbol *_variable;
	const RefsSlice &_next_pattern;
	const Slice &_sequence;

	bool heads(size_t n, const Symbol *head) const;
	bool consume(size_t n) const;
	bool match_variable(size_t match_size, const BaseExpressionRef &item) const;

public:
	inline MatcherImplementation(MatchContext &context, const BaseExpressionRef &this_pattern, const RefsSlice &next_pattern, const Slice &sequence) :
		_context(context), _variable(nullptr), Matcher(this_pattern), _next_pattern(next_pattern), _sequence(sequence) {
	}

	inline MatcherImplementation(MatchContext &context, const Symbol *variable, const BaseExpressionRef &this_pattern, const RefsSlice &next_pattern, const Slice &sequence) :
		_context(context), _variable(variable), Matcher(this_pattern), _next_pattern(next_pattern), _sequence(sequence) {
	}

	virtual size_t sequence_size() const {
		return _sequence.size();
	}

	virtual BaseExpressionRef sequence_next() const {
		return _sequence[0];
	}

	/*inline const Slice &sequence() const {
		return _sequence;
	}*/

	virtual bool operator()(size_t match_size, const Symbol *head) const;

	virtual bool operator()(const Symbol *var, const BaseExpressionRef &pattern) const;

	virtual bool descend() const;

	virtual bool blank_sequence(match_size_t k, const Symbol *head) const;
};

#include "leaves.h"

inline Match match(const BaseExpressionRef &patt, const BaseExpressionRef &item, Definitions &definitions) {
	MatchContext context(patt, item, definitions);
	{
		MatcherImplementation<RefsSlice> matcher(context, patt, empty_slice, RefsSlice(item));

		if (patt->match_sequence(matcher)) {
			return Match(true, context);
		} else {
			return Match(); // no match
		}
	}
}

#endif //CMATHICS_MATCHER_H
