#pragma once
// @formatter:off

template<typename Arguments, typename Options>
inline BaseExpressionRef RewriteBaseExpression::rewrite_or_copy(
    const BaseExpressionRef &expr,
    const Arguments &args,
    const Options &options,
    const Evaluation &evaluation) const {

    const index_t slot = m_slot;

    if (slot >= 0) {
        return args(slot, expr);
    } else switch (slot) {
        case Copy:
            return expr;

        case Descend:
            return m_down->rewrite_or_copy(
                args, options, evaluation);

        case OptionValue: {
            const auto option = options(m_option_value);
            if (option) {
                return option;
            }
            return expr;
        }

        case IllegalSlot: {
            evaluation.message(
                evaluation.Function, "slot", m_illegal_slot);
            return expr;
        }

        default:
            assert(false);
            return expr;
    }
};

template<typename Arguments>
std::vector<const RewriteBaseExpression> RewriteExpression::nodes(
    Arguments &arguments,
    const Expression *expr,
    Definitions &definitions) {

    return expr->with_slice([&arguments, &definitions] (const auto &slice) {
        std::vector<const RewriteBaseExpression> refs;
        const size_t n = slice.size();
        refs.reserve(n);
        for (size_t i = 0; i < n; i++) {
            refs.emplace_back(RewriteBaseExpression::construct(arguments, slice[i], definitions));
        }
        return refs;
    });
}

inline ExpressionRef RewriteExpression::rewrite_functions(
    const ExpressionPtr expr,
    const RewriteMask mask,
    Definitions &definitions) {

    if (expr->head()->symbol() == S::Function && (mask & SlotRewriteMask) && expr->size() >= 2) {
        return expr->with_slice([expr, &definitions] (const auto &slice) -> ExpressionRef {
            TempVector names;
            ArgumentsMap renames;

            const auto add = [&names, &renames, &definitions] (SymbolPtr arg) {
                std::string name(arg->as_symbol()->name());
                name.append("$");
                const BaseExpressionRef new_arg = definitions.lookup(name.c_str());
                renames[arg->as_symbol()] = BaseExpressionRef(new_arg);
                names.push_back(new_arg);
            };

            const BaseExpressionRef args = slice[0];
            if (args->is_expression() && args->as_expression()->head()->symbol() == S::List) {
                args->as_expression()->with_slice([&names, &renames, &add](const auto &args) {
                    for (auto arg : args) {
                        if (arg->is_symbol()) {
                            add(arg->as_symbol());
                        } else {
                            names.push_back(BaseExpressionRef(arg));
                        }
                    }
                });
            } else if (args->is_symbol()) {
                add(args->as_symbol());
            } else {
                return expr;
            }

            const auto &symbols = definitions.symbols();

            const BaseExpressionRef new_args = names.to_expression(symbols.List);

            const BaseExpressionRef new_body =
                coalesce(BaseExpressionRef(slice[1]->replace_all(renames)), slice[1]);

            return expression(symbols.Function, sequential([&new_args, &new_body, &slice] (auto &store) {
                store(BaseExpressionRef(new_args));
                store(BaseExpressionRef(new_body));
                for (size_t i = 2; i < slice.size(); i++) {
                    store(BaseExpressionRef(slice[i]));
                }
            }, slice.size()));
        });
    } else {
        return ExpressionRef();
    }
}

template<typename Arguments>
RewriteExpressionRef RewriteExpression::construct(
    Arguments &arguments,
    const Expression *expr,
    Definitions &definitions,
    bool is_rewritten) {

    const RewriteBaseExpression head(
        RewriteBaseExpression::construct(arguments, expr->head(), definitions));
    std::vector<const RewriteBaseExpression> leaves(
        RewriteExpression::nodes(arguments, expr, definitions));

    RewriteMask mask = head.mask();
    for (const RewriteBaseExpression &leaf : leaves) {
        mask |= leaf.mask();
    }

    if (is_rewritten) {
        const ExpressionRef new_expr(rewrite_functions(expr, mask, definitions));

        // if a rewrite of the expression happened, we actually also need to redo
        // all our RewriteBaseExpressions (since the embedded RewritesExpressions
        // in RewriteBaseExpression::m_down need updating too).

        if (new_expr) {
            return construct(arguments, new_expr.get(), definitions, true);
        }
    }

    return RewriteExpressionRef(
        new RewriteExpression(head, std::move(leaves), mask, expr));
}

template<typename Arguments, typename Options>
inline BaseExpressionRef RewriteExpression::rewrite_or_copy(
    const Arguments &args,
    const Options &options,
    const Evaluation &evaluation) const {

    return m_expr->with_slice_c(
        [this, &args, &options, &evaluation] (const auto &slice) {
            const auto generate = [this, &slice, &args, &options, &evaluation] (auto &store) {
                const size_t n = slice.size();
                for (size_t i = 0; i < n; i++) {
                    store(m_leaves[i].rewrite_or_copy(slice[i], args, options, evaluation));
                }
            };

            return ExpressionRef(expression(
                m_head.rewrite_or_copy(m_expr->head(), args, options, evaluation),
                sequential(generate, slice.size())))->flatten_sequence_or_copy();
        });
}
