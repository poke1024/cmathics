#ifndef CMATHICS_CACHE_H
#define CMATHICS_CACHE_H

#include <unordered_set>

class RewriteExpression;

typedef ConstSharedPtr<RewriteExpression> RewriteExpressionRef;
typedef ConstSharedPtr<RewriteExpression> ConstRewriteExpressionRef;
typedef QuasiConstSharedPtr<RewriteExpression> CachedRewriteExpressionRef;
typedef UnsafeSharedPtr<RewriteExpression> UnsafeRewriteExpressionRef;

class RewriteBaseExpression {
private:
	const index_t m_slot;
	const RewriteExpressionRef m_down;

	inline RewriteBaseExpression(
		index_t slot,
		RewriteExpressionRef down = RewriteExpressionRef()) :

		m_slot(slot),
		m_down(down){
	}

public:
	inline RewriteBaseExpression(
		const RewriteBaseExpression &other) :

		m_slot(other.m_slot),
		m_down(other.m_down) {
	 }

	inline RewriteBaseExpression(
		RewriteBaseExpression &&other) :

		m_slot(other.m_slot),
		m_down(other.m_down) {
	}

	template<typename Arguments>
	static inline RewriteBaseExpression construct(
		Arguments &arguments,
		const BaseExpressionRef &expr);

	template<typename Arguments>
	inline BaseExpressionRef rewrite_or_copy(
		const BaseExpressionRef &expr,
		const Arguments &args) const;
};

class Rewrite;

typedef ConstSharedPtr<Rewrite> RewriteRef;
typedef QuasiConstSharedPtr<Rewrite> CachedRewriteRef;
typedef UnsafeSharedPtr<Rewrite> UnsafeRewriteRef;

class Rewrite : public RewriteBaseExpression, public Shared<Rewrite, SharedHeap> {
public:
	using RewriteBaseExpression::RewriteBaseExpression;

	inline Rewrite(
		RewriteBaseExpression &&other) : RewriteBaseExpression(other) {
	}

	template<typename Arguments>
	static inline RewriteRef construct(
		Arguments &arguments,
		const BaseExpressionRef &expr);
};

class RewriteExpression : public Shared<RewriteExpression, SharedHeap> {
private:
    const RewriteBaseExpression m_head;
	const std::vector<const RewriteBaseExpression> m_leaves;

    template<typename Arguments>
    static std::vector<const RewriteBaseExpression> nodes(
        Arguments &arguments,
        const Expression *body_ptr);

public:
    template<typename Arguments>
    RewriteExpression(
        Arguments &arguments,
        const Expression *body_ptr);

	template<typename Arguments>
    inline BaseExpressionRef rewrite_or_copy(
        const Expression *body,
        const Arguments &args) const;
};

template<typename Arguments>
inline RewriteBaseExpression RewriteBaseExpression::construct(
	Arguments &arguments,
	const BaseExpressionRef &expr) {

	const SlotDirective directive = arguments(expr);

	switch (directive.m_action) {
		case SlotDirective::Slot:
			return RewriteBaseExpression(directive.m_slot);
		case SlotDirective::Copy:
			return RewriteBaseExpression(-1);
		case SlotDirective::Descend:
			if (expr->type() == ExpressionType) {
				return RewriteBaseExpression(-1, RewriteExpressionRef(
					new RewriteExpression(arguments, expr->as_expression())));
			} else {
				return RewriteBaseExpression(-1);
			}
		default:
			throw std::runtime_error("invalid SlotDirective");
	}
}

template<typename Arguments>
inline BaseExpressionRef RewriteBaseExpression::rewrite_or_copy(
	const BaseExpressionRef &expr,
	const Arguments &args) const {

	const index_t slot = m_slot;
	if (slot >= 0) {
		return args(slot, expr);
	} else {
		const RewriteExpressionRef &down = m_down;
		if (down) {
			return down->rewrite_or_copy(
				expr->as_expression(), args);
		} else {
			return expr;
		}
	}
};

template<typename Arguments>
inline RewriteRef Rewrite::construct(
	Arguments &arguments,
	const BaseExpressionRef &expr) {
	return new Rewrite(std::move(RewriteBaseExpression::construct(arguments, expr)));
}

class SlotFunction;

typedef QuasiConstSharedPtr<SlotFunction> CachedSlotFunctionRef;
typedef ConstSharedPtr<SlotFunction> ConstSlotFunctionRef;
typedef UnsafeSharedPtr<SlotFunction> UnsafeSlotFunctionRef;

struct SlotFunction : public Shared<SlotFunction, SharedHeap> {
private:
	const RewriteExpressionRef m_rewrite;
    const size_t m_slot_count;

	inline SlotFunction(
		const RewriteExpressionRef &function,
		size_t slot_count) :

		m_rewrite(function),
		m_slot_count(slot_count) {
	}

public:
	inline SlotFunction(
		SlotFunction &&other) :

		m_rewrite(other.m_rewrite),
		m_slot_count(other.m_slot_count) {
	}

	static UnsafeSlotFunctionRef construct(const Expression *body);

	template<typename Arguments>
    inline BaseExpressionRef rewrite_or_copy(
        const Expression *body,
	    const Arguments &args,
        size_t n_args) const;
};

class Matcher;

PatternMatcherRef compile_expression_pattern(const BaseExpressionRef &patt);
PatternMatcherRef compile_string_pattern(const BaseExpressionRef &patt);

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

	inline RewriteRef rewrite(const PatternMatcherRef &matcher, const BaseExpressionRef &rhs) { // concurrent.
        return RewriteRef(m_rewrite.ensure([&matcher, &rhs] () {
            CompiledArguments arguments(matcher->variables());
            return Rewrite::construct(arguments, rhs);
        }));
	}
};

#endif //CMATHICS_CACHE_H
