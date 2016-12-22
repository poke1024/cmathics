#ifndef CMATHICS_CACHE_H
#define CMATHICS_CACHE_H

#include <unordered_set>

class FunctionBody;

typedef SharedPtr<FunctionBody> FunctionBodyRef;

class FunctionBody : public Shared<FunctionBody, SharedHeap> {
public:
    class Node {
    private:
        index_t m_slot;
        FunctionBodyRef m_down;

    public:
        template<typename Arguments>
        Node(
            Arguments &arguments,
            const BaseExpressionRef &expr);

        template<typename Arguments>
        inline BaseExpressionRef replace_or_copy(
            const BaseExpressionRef &expr,
            const Arguments &args) const {

            const index_t slot = m_slot;
            if (slot >= 0) {
                return args(slot, expr);
            } else {
                const FunctionBodyRef &down = m_down;
                if (down) {
                    return down->replace_or_copy(
						expr->as_expression(), args);
                } else {
                    return expr;
                }
            }
        };
    };

private:
    const Node m_head;
	const std::vector<const Node> m_leaves;

    template<typename Arguments>
    static std::vector<const Node> nodes(
        Arguments &arguments,
        const Expression *body_ptr);

public:
    template<typename Arguments>
    FunctionBody(
        Arguments &arguments,
        const Expression *body_ptr);

	template<typename Arguments>
    inline BaseExpressionRef replace_or_copy(
        const Expression *body,
        const Arguments &args) const;
};

template<typename Arguments>
FunctionBody::Node::Node(
	Arguments &arguments,
	const BaseExpressionRef &expr) {

	const SlotDirective directive = arguments(expr);

	switch (directive.m_action) {
		case SlotDirective::Slot:
			m_slot = directive.m_slot;
			break;
		case SlotDirective::Copy:
			m_slot = -1;
			break;
		case SlotDirective::Descend:
			m_slot = -1;
			if (expr->type() == ExpressionType) {
				m_down = new FunctionBody(
					arguments, expr->as_expression());
			}
			break;
		default:
			throw std::runtime_error("invalid SlotDirective");
	}
}

struct SlotFunction : public Shared<SlotFunction, SharedHeap> {
private:
    size_t m_slot_count;
    FunctionBodyRef m_function;

public:
    SlotFunction(const Expression *body);

	template<typename Arguments>
    inline BaseExpressionRef replace_or_copy(
        const Expression *body,
	    const Arguments &args,
        size_t n_args) const;
};

typedef SharedPtr<SlotFunction> SlotFunctionRef;

class Cache : public Shared<Cache, SharedPool> {
public:
    SlotFunctionRef slot_function;
    FunctionBodyRef vars_function;
	PatternMatcherRef expression_matcher;
	PatternMatcherRef string_matcher;
};

#endif //CMATHICS_CACHE_H
