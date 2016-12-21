#ifndef CMATHICS_CACHE_H
#define CMATHICS_CACHE_H

#include <unordered_set>

class PatternMatcher;

typedef boost::intrusive_ptr<PatternMatcher> PatternMatcherRef;

class FunctionBody;

typedef std::shared_ptr<FunctionBody> FunctionBodyRef;

class FunctionBody {
private:
    class Node {
    private:
        index_t m_slot;
        FunctionBodyRef m_down;

    public:
        template<typename Arguments>
        Node(
            Arguments &arguments,
            const BaseExpressionRef &expr);

        inline index_t slot() const {
            return m_slot;
        }

        inline const FunctionBodyRef &down() const {
            return m_down;
        }
    };

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
    inline BaseExpressionRef instantiate(
        const Expression *body,
        const Arguments &args) const;
};

struct SlotDirective {
	enum Action {
		Slot,
		Copy,
		Descend
	};

	const Action m_action;
	const index_t m_slot;

	inline SlotDirective(Action action, index_t slot = 0) :
		m_action(action), m_slot(slot) {
	}

	inline static SlotDirective slot(index_t slot) {
		return SlotDirective(SlotDirective::Slot, slot);
	}

	inline static SlotDirective copy() {
		return SlotDirective(SlotDirective::Copy);
	}

	inline static SlotDirective descend() {
		return SlotDirective(SlotDirective::Descend);
	}
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
				m_down = std::make_shared<FunctionBody>(
						arguments, expr->as_expression());
			}
			break;
	}
}

struct SlotFunction {
private:
    size_t m_slot_count;
    FunctionBodyRef m_function;

public:
    SlotFunction(const Expression *body);

	template<typename Arguments>
    inline BaseExpressionRef instantiate(
        const Expression *body,
	    const Arguments &args,
        size_t n_args) const;
};

typedef std::shared_ptr<SlotFunction> SlotFunctionRef;

class Cache {
protected:
	std::atomic<size_t> m_ref_count;

	friend void intrusive_ptr_add_ref(Cache *cache);
	friend void intrusive_ptr_release(Cache *cache);

public:
    SlotFunctionRef slot_function;
    FunctionBodyRef vars_function;
	PatternMatcherRef expression_matcher;
	PatternMatcherRef string_matcher;

	inline Cache() : m_ref_count(0) {
	}
};

typedef boost::intrusive_ptr<Cache> CacheRef;

#endif //CMATHICS_CACHE_H
