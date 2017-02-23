#pragma once

class RewriteExpression;

typedef ConstSharedPtr<RewriteExpression> RewriteExpressionRef;
typedef ConstSharedPtr<RewriteExpression> ConstRewriteExpressionRef;
typedef QuasiConstSharedPtr<RewriteExpression> CachedRewriteExpressionRef;
typedef UnsafeSharedPtr<RewriteExpression> UnsafeRewriteExpressionRef;

// RewriteMask is a bit field, indicating a Slot with (1 << 0), a Copy
// with (1 << -RewriteBaseExpression::Copy), and so on.
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
    static inline RewriteBaseExpression construct(
        Arguments &arguments,
        const BaseExpressionRef &expr);

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
	const RewriteMask m_mask;

    template<typename Arguments>
    static std::vector<const RewriteBaseExpression> nodes(
        Arguments &arguments,
        const Expression *body_ptr);

	inline static RewriteMask compute_mask(
		const RewriteBaseExpression &head,
		const std::vector<const RewriteBaseExpression> &leaves) {

		RewriteMask mask = head.mask();
		for (const RewriteBaseExpression &leaf : leaves) {
			mask |= leaf.mask();
		}
		return mask;
	}

public:
    template<typename Arguments>
    RewriteExpression(
        Arguments &arguments,
        const Expression *body_ptr);

    template<typename Arguments, typename Options>
    inline BaseExpressionRef rewrite_or_copy(
        const Expression *body,
        const Arguments &args,
        const Options &options,
        const Evaluation &evaluation) const;

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
inline RewriteBaseExpression RewriteBaseExpression::construct(
    Arguments &arguments,
    const BaseExpressionRef &expr) {

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
	            const RewriteExpressionRef rewrite(
			         new RewriteExpression(arguments, expr->as_expression()));

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
    const RewriteRef m_rewrite;
    const size_t m_slot_count;

    inline SlotFunction(
        const RewriteRef &rewrite,
        size_t slot_count) :

        m_rewrite(rewrite),
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
        size_t n_args,
        const Evaluation &evaluation) const;
};
