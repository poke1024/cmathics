#include "types.h"
#include "expression.h"
#include "pattern.h"
#include "matcher.h"

index_t DynamicOptionsProcessor::match(
	const SlowLeafSequence &sequence,
	index_t begin,
	index_t end,
	const MatchRest &rest) {

	return do_match(sequence, begin, end, rest);
}

index_t DynamicOptionsProcessor::match(
	const FastLeafSequence &sequence,
	index_t begin,
	index_t end,
	const MatchRest &rest) {

	return do_match(sequence, begin, end, rest);
}
