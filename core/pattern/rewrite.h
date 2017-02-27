#pragma once

// so why all this complex code around RewriteBaseExpression when it
// would be quite easy to provide a replace_slots and replace_vars as
// virtual functions to BaseExpression? this implements a sort of simple
// precompiler for "expressions that are partially replaced" and, well,
// some of this might look like a pretty debatable design decision.

// the original idea to introduce this was Map. an extremely common use case
// is lambda& /@ x, so lambda needs to be evaluated as fast as possible as it
// is applied to hundreds or millions of elements. also, lambda may be nested
// and deep, and it's sort of waste of time to always copy the whole tree if
// only one shallow slot is manipulated. but we don't know if we don't analyze
// lambda once at the beginning. this thing is also useful for ReplaceAll.

// on the other hand, many common use cases actually suffer a bit from this
// RewriteBaseExpression design, as everything needs to be built before it
// can be applied, which basically slows everything as compared to a regular
// replace_slots() by a factor of 2; which is not too bad; but still, it's
// debatable whether this outweights its use in the Map case. I'm not sure.

class RewriteExpression;

typedef ConstSharedPtr<RewriteExpression> RewriteExpressionRef;
typedef ConstSharedPtr<RewriteExpression> ConstRewriteExpressionRef;
typedef QuasiConstSharedPtr<RewriteExpression> CachedRewriteExpressionRef;
typedef UnsafeSharedPtr<RewriteExpression> UnsafeRewriteExpressionRef;

// RewriteMask is a bit field, indicating a Slot with (1 << 0), a Copy with
// (1 << -RewriteBaseExpression::Copy), and so on.
using RewriteMask = uint16_t;

enum {
	SlotRewriteMask = 1L << 0
};

class RewriteBaseExpression {
public:
    enum { // used in m_slot
        Copy = -1,
        Descend = -2,
        OptionValue = -3,
        IllegalSlot = -4
    };

	static constexpr RewriteMask create_mask(index_t slot) {
		return slot >= 0 ? SlotRewriteMask : (1L << (-slot));
	}

private:
    const index_t m_slot;
    const SymbolRef m_option_value;
    const RewriteExpressionRef m_down;
    const BaseExpressionRef m_illegal_slot;
	const RewriteMask m_mask;

    inline RewriteBaseExpression(
        index_t slot,
        const RewriteExpressionRef &down = RewriteExpressionRef(),
        const SymbolRef &option_value = SymbolRef(),
        const BaseExpressionRef &illegal_slot = BaseExpressionRef());

public:
    inline RewriteBaseExpression(
        const RewriteBaseExpression &other) :

        m_slot(other.m_slot),
        m_down(other.m_down),
        m_option_value(other.m_option_value),
        m_illegal_slot(other.m_illegal_slot),
	    m_mask(other.m_mask) {
    }

    inline RewriteBaseExpression(
        RewriteBaseExpression &&other) :

        m_slot(other.m_slot),
        m_down(other.m_down),
        m_option_value(other.m_option_value),
        m_illegal_slot(other.m_illegal_slot),
	    m_mask(other.m_mask) {
    }

    template<typename Arguments>
    static inline RewriteBaseExpression from_arguments(
        Arguments &arguments,
        const BaseExpressionRef &expr,
        Definitions &definitions);

    template<typename Arguments, typename Options>
    inline BaseExpressionRef rewrite_or_copy(
        const BaseExpressionRef &expr,
        const Arguments &args,
        const Options &options,
        const Evaluation &evaluation) const;

    template<typename Arguments>
    inline BaseExpressionRef rewrite_root_or_copy(
        const BaseExpressionRef &expr,
        const Arguments &args,
        const OptionsMap *options,
        const Evaluation &evaluation) const;

	inline RewriteMask mask() const {
		return m_mask;
	}
};

class Rewrite;

typedef ConstSharedPtr<Rewrite> RewriteRef;
typedef QuasiConstSharedPtr<Rewrite> CachedRewriteRef;
typedef UnsafeSharedPtr<Rewrite> UnsafeRewriteRef;

class Rewrite : public RewriteBaseExpression, public HeapObject<Rewrite> {
public:
    using RewriteBaseExpression::RewriteBaseExpression;

    inline Rewrite(
        RewriteBaseExpression &&other) : RewriteBaseExpression(other) {
    }

    template<typename Arguments>
    static inline RewriteRef from_arguments(
        Arguments &arguments,
        const BaseExpressionRef &expr,
        Definitions &definitions);
};

class RewriteExpression : public HeapObject<RewriteExpression> {
private:
    const RewriteBaseExpression m_head;
    const std::vector<const RewriteBaseExpression> m_leaves;
	const RewriteMask m_mask;
    const ExpressionRef m_expr;

    template<typename Arguments>
    static std::vector<const RewriteBaseExpression> nodes(
        Arguments &arguments,
        const Expression *expr,
        Definitions &definitions);

    inline static ExpressionRef rewrite_functions(
        const ExpressionPtr expr,
        const RewriteMask mask,
        Definitions &definitions);

public:
    inline RewriteExpression(
        const RewriteBaseExpression &head,
        std::vector<const RewriteBaseExpression> &&leaves,
        const RewriteMask mask,
        const ExpressionPtr expr) :

        m_head(head),
        m_leaves(leaves),
        m_mask(mask),
        m_expr(expr) {
    }

    template<typename Arguments, typename Options>
    inline BaseExpressionRef rewrite_or_copy(
        const Arguments &args,
        const Options &options,
        const Evaluation &evaluation) const;

    template<typename Arguments>
    static RewriteExpressionRef from_arguments(
        Arguments &arguments,
        const Expression *expr,
        Definitions &definitions,
        bool is_rewritten = false);

	inline RewriteMask mask() const {
		return m_mask;
	}

	inline bool is_pure_copy() const {
		constexpr RewriteMask mask =
			RewriteBaseExpression::create_mask(RewriteBaseExpression::Copy) |
			RewriteBaseExpression::create_mask(RewriteBaseExpression::Descend);
		return (m_mask & mask) == m_mask;
	}
};

inline RewriteBaseExpression::RewriteBaseExpression(
	index_t slot,
	const RewriteExpressionRef &down,
	const SymbolRef &option_value,
	const BaseExpressionRef &illegal_slot) :

	m_slot(slot),
	m_down(down),
	m_option_value(option_value),
	m_illegal_slot(illegal_slot),
	m_mask(create_mask(slot) | (m_down ? m_down->mask() : 0)) {
}

template<typename Arguments>
inline RewriteBaseExpression RewriteBaseExpression::from_arguments(
    Arguments &arguments,
    const BaseExpressionRef &expr,
    Definitions &definitions) {

    const SlotDirective directive = arguments(expr);

    switch (directive.m_action) {
        case SlotDirective::Slot:
            return RewriteBaseExpression(directive.m_slot);

        case SlotDirective::OptionValue:
            return RewriteBaseExpression(
                RewriteBaseExpression::OptionValue,
                RewriteExpressionRef(),
                directive.m_option_value);

        case SlotDirective::Copy:
            return RewriteBaseExpression(
                RewriteBaseExpression::Copy);

        case SlotDirective::Descend:
            if (expr->is_expression()) {
	            const RewriteExpressionRef rewrite(RewriteExpression::from_arguments(
                     arguments, expr->as_expression(), definitions));

	            if (!rewrite->is_pure_copy()) {
		            return RewriteBaseExpression(
				        RewriteBaseExpression::Descend, rewrite);
	            }
            }

            return RewriteBaseExpression(
                RewriteBaseExpression::Copy);

        case SlotDirective::IllegalSlot:
            return RewriteBaseExpression(
                RewriteBaseExpression::IllegalSlot,
                RewriteExpressionRef(),
                SymbolRef(),
                directive.m_illegal_slot);

        default:
            throw std::runtime_error("invalid SlotDirective");
    }
}

template<typename Arguments>
inline BaseExpressionRef RewriteBaseExpression::rewrite_root_or_copy(
    const BaseExpressionRef &expr,
    const Arguments &args,
    const OptionsMap *options,
    const Evaluation &evaluation) const {

    if (!options || options->empty()) {
        return rewrite_or_copy(expr, args, [] (const SymbolRef &name) {
            return BaseExpressionRef();
        }, evaluation);
    } else {
        return rewrite_or_copy(expr, args, [options] (const SymbolRef &name) {
            const auto i = options->find(name);
            if (i != options->end()) {
                return BaseExpressionRef(i->second);
            } else {
                return BaseExpressionRef();
            }
        }, evaluation);
    }
}

template<typename Arguments>
inline RewriteRef Rewrite::from_arguments(
    Arguments &arguments,
    const BaseExpressionRef &expr,
    Definitions &definitions) {

    return Rewrite::construct(std::move(
        RewriteBaseExpression::from_arguments(arguments, expr, definitions)));
}

class SlotFunction;

typedef QuasiConstSharedPtr<SlotFunction> CachedSlotFunctionRef;
typedef ConstSharedPtr<SlotFunction> ConstSlotFunctionRef;
typedef UnsafeSharedPtr<SlotFunction> UnsafeSlotFunctionRef;

class SlotFunction : public HeapObject<SlotFunction> {
private:
    const RewriteRef m_rewrite;
    const size_t m_slot_count;

public:
    inline SlotFunction(
        const RewriteRef &rewrite,
        size_t slot_count) :

        m_rewrite(rewrite),
        m_slot_count(slot_count) {
    }

    inline SlotFunction(
        SlotFunction &&other) :

        m_rewrite(other.m_rewrite),
        m_slot_count(other.m_slot_count) {
    }

    static UnsafeSlotFunctionRef from_expression(
        const Expression *body,
        Definitions &definitions);

    template<typename Arguments>
    inline BaseExpressionRef rewrite_or_copy(
        const Expression *body,
        const Arguments &args,
        size_t n_args,
        const Evaluation &evaluation) const;
};
