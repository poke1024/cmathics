#ifndef CMATHICS_CACHE_H
#define CMATHICS_CACHE_H

#include <unordered_set>
#include "pattern/rewrite.h"
#include "pattern/matcher.h"

class Cache : public Shared<Cache, SharedPool> {
private:
	CachedRewriteRef m_rewrite;

	CachedPatternMatcherRef m_expression_matcher;
	CachedPatternMatcherRef m_string_matcher;

public:
	CachedSlotFunctionRef slot_function;
	CachedRewriteExpressionRef vars_function;

	inline PatternMatcherRef expression_matcher(BaseExpressionPtr expr) { // concurrent.
        return PatternMatcherRef(m_expression_matcher.ensure([expr] () {
            return compile_expression_pattern(BaseExpressionRef(expr));
        }));
	}

	inline PatternMatcherRef string_matcher(BaseExpressionPtr expr) { // concurrent.
        return PatternMatcherRef(m_string_matcher.ensure([expr] () {
            return compile_string_pattern(BaseExpressionRef(expr));
        }));
	}

	inline RewriteRef rewrite(
        const PatternMatcherRef &matcher,
        const BaseExpressionRef &rhs,
        const Evaluation &evaluation); // concurrent.
};

#endif //CMATHICS_CACHE_H
