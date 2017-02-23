#pragma once

class RewriteExpression;

typedef ConstSharedPtr<RewriteExpression> RewriteExpressionRef;
typedef ConstSharedPtr<RewriteExpression> ConstRewriteExpressionRef;
typedef QuasiConstSharedPtr<RewriteExpression> CachedRewriteExpressionRef;
typedef UnsafeSharedPtr<RewriteExpression> UnsafeRewriteExpressionRef;

class RewriteBaseExpression {
public:
    enum { // used in m_slot
        Copy = -1,
        Descend = -2,
        OptionValue = -3,
        IllegalSlot = -4
    };

private:
    const index_t m_slot;
    const SymbolRef m_option_value;
    const RewriteExpressionRef m_down;
    const BaseExpressionRef m_illegal_slot;

    inline RewriteBaseExpression(
        index_t slot,
        const RewriteExpressionRef &down = RewriteExpressionRef(),
        const SymbolRef &option_value = SymbolRef(),
        const BaseExpressionRef &illegal_slot = BaseExpressionRef()) :

        m_slot(slot),
        m_down(down),
        m_option_value(option_value),
        m_illegal_slot(illegal_slot) {
    }

public:
    inline RewriteBaseExpression(
        const RewriteBaseExpression &other) :

        m_slot(other.m_slot),
        m_down(other.m_down),
        m_option_value(other.m_option_value),
        m_illegal_slot(other.m_illegal_slot) {
    }

    inline RewriteBaseExpression(
        RewriteBaseExpression &&other) :

        m_slot(other.m_slot),
        m_down(other.m_down),
        m_option_value(other.m_option_value),
        m_illegal_slot(other.m_illegal_slot) {
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

    template<typename Arguments, typename Options>
    inline BaseExpressionRef rewrite_or_copy(
        const Expression *body,
        const Arguments &args,
        const Options &options,
        const Evaluation &evaluation) const;
};

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
                return RewriteBaseExpression(
                    RewriteBaseExpression::Descend,
                    RewriteExpressionRef(new RewriteExpression(
                        arguments, expr->as_expression())));
            } else {
                return RewriteBaseExpression(
                    RewriteBaseExpression::Copy);
            }
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
