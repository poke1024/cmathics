inline bool is_infinity(const Expression *expr) {
    if (expr->head()->symbol() != S::DirectedInfinity) {
        return false;
    }
    if (expr->size() != 1) {
        return false;
    }
    if (!expr->n_leaves<1>()[0]->is_one()) {
        return false;
    }
    return true;
}

class Levelspec {
public:
    class InvalidError : public std::runtime_error {
    public:
        InvalidError() : std::runtime_error("invalid levelspec") {
        }
    };

    struct Position {
        const Position *up;
        size_t index;

        inline void set_up(const Position *p) {
            up = p;
        }

        inline void set_index(size_t i) {
            index = i;
        }
    };

    struct NoPosition {
        inline void set_up(const NoPosition *p) {
        }

        inline void set_index(size_t i) {
        }
    };

    struct Immutable : public nothing {
        inline constexpr operator bool() const {
            return false;
        }

        inline operator BaseExpressionRef() const {
            return BaseExpressionRef();
        }

        inline Immutable &operator=(const Immutable&) {
            return *this;
        }

        inline Immutable &operator=(const ExpressionRef&) {
            return *this;
        }
    };

private:
    optional<machine_integer_t> m_start;
    optional<machine_integer_t> m_stop;

    static inline optional<machine_integer_t> value_to_level(const BaseExpressionRef &item) {
        switch (item->type()) {
            case MachineIntegerType:
                return static_cast<const MachineInteger*>(item.get())->value;
            case ExpressionType:
                if (is_infinity(item->as_expression())) {
                    return optional<machine_integer_t>();
                } else {
                    throw InvalidError();
                }
            default:
                throw InvalidError();
        }
    }

public:
    Levelspec(BaseExpressionPtr spec) {
        if (spec->is_list()) {
            const ExpressionPtr list = spec->as_expression();

            switch (list->size()) {
                case 1: {
                    const auto *leaves = list->n_leaves<1>();
                    const optional<machine_integer_t> i = value_to_level(leaves[0]);
                    m_start = i;
                    m_stop = i;
                    break;
                }
                case 2: {
                    const auto *leaves = list->n_leaves<2>();
                    m_start = value_to_level(leaves[0]);
                    m_stop = value_to_level(leaves[1]);
                    break;
                }
                default:
                    throw InvalidError();
            }
        } else if (spec->symbol() == S::All) {
            m_start = 0;
            m_stop = optional<machine_integer_t>();
        } else {
            m_start = 1;
            m_stop = value_to_level(spec);
        }
    }

    inline bool is_in_level(index_t current, index_t depth) const {
        index_t start;
        if (m_start) {
            start = *m_start;
        } else {
            return false;
        }
        index_t stop;
        if (m_stop) {
            stop = *m_stop;
        } else {
            stop = current;
        }
        if (start < 0) {
            start += current + depth + 1;
        }
        if (stop < 0) {
            stop += current + depth + 1;
        }
        return start <= current && current <= stop;
    }

    template<typename CallbackType, typename PositionType, typename Callback>
    std::tuple<CallbackType, size_t> walk(
        const BaseExpressionRef &node,
        const bool heads,
        const Callback &callback,
        const Evaluation &evaluation,
        const index_t current = 0,
        const PositionType *pos = nullptr) const {

        constexpr bool immutable = std::is_same<CallbackType, Immutable>::value;

        // CompileToSliceType comes with a certain cost (one vcall) and it's usually only worth it if
        // we actually construct new expressions. if we don't change anything, DoNotCompileToSliceType
        // is the faster choice (no vcall except for packed slices).

        constexpr SliceMethodOptimizeTarget Optimize =
            immutable ? DoNotCompileToSliceType : CompileToSliceType;

        size_t depth = 0;
        CallbackType modified_node;

        if (node->is_expression()) {
            const Expression * const expr =
                static_cast<const Expression*>(node.get());

            const BaseExpressionRef &head = expr->head();
            CallbackType new_head;

            if (heads) {
                PositionType head_pos;

                head_pos.set_up(pos);
                head_pos.set_index(0);

                new_head = std::get<0>(walk<CallbackType, PositionType>(
                    expr->head(), heads, callback, evaluation, current + 1, &head_pos));
            }

            PositionType new_pos;
            new_pos.set_up(pos);

            auto apply = [this, heads, current, &callback, &head, &new_head, &new_pos, &evaluation, &depth]
                (const auto &slice) mutable  {

                auto recurse =
                    [this, heads, current, &callback, &new_pos, &evaluation, &depth]
                        (size_t index0, const BaseExpressionRef &leaf) mutable {

                        new_pos.set_index(index0);

                        std::tuple<CallbackType, size_t> result = this->walk<CallbackType, PositionType>(
                            leaf, heads, callback, evaluation, current + 1, &new_pos);

                        depth = std::max(std::get<1>(result) + 1, depth);

                        return std::get<0>(result);
                    };

                return conditional_map_indexed(
                    replace_head(head, new_head),
                    lambda(recurse),
                    slice,
                    evaluation);
            };

            modified_node = expr->with_slice_implementation<Optimize>(apply);
        }

        if (is_in_level(current, depth)) {
            CallbackType result = coalesce(
                callback(modified_node ? modified_node : node, *pos), modified_node);
            return std::make_tuple(result, depth);
        } else {
            return std::make_tuple(modified_node, depth);
        }
    }

    template<typename Callback>
    std::tuple<Levelspec::Immutable, size_t>
    inline walk_immutable(
        const BaseExpressionRef &node,
        const bool heads,
        const Callback &callback,
        const Evaluation &evaluation) const {

        return walk<Levelspec::Immutable, Levelspec::NoPosition>(
            node,
            heads,
            [&callback] (const BaseExpressionRef &node, Levelspec::NoPosition) {
                callback(node);
                return Levelspec::Immutable();
            },
            evaluation);
    };
};
