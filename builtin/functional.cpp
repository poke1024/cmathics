#include "functional.h"

// as Function is a very important pattern, we provide a special optimized Rule for it.

BaseExpressionRef function_pattern(
	const SymbolRef &head,
    const Definitions &definitions);

class SlotArguments {
private:
    index_t m_slot_count;

public:
    SlotArguments() : m_slot_count(0) {
    }

    inline size_t slot_count() const {
        return m_slot_count;
    }

    SlotDirective operator()(const BaseExpressionRef &item) {
        if (item->type() != ExpressionType) {
            return SlotDirective::copy();
        }

        const Expression *expr = item->as_expression();
        const BaseExpressionRef &head = expr->head();

        switch (head->extended_type()) {
            case SymbolSlot:
                if (expr->size() != 1) {
                    throw std::runtime_error("Slot must contain one leaf");
                } else {
                    const BaseExpressionRef *leaves = expr->static_leaves<1>();
                    const BaseExpressionRef &slot = leaves[0];

                    if (slot->type() == MachineIntegerType) {
                        const machine_integer_t slot_id =
                            static_cast<const MachineInteger *>(slot.get())->value;
                        if (slot_id < 1) {
                            throw std::runtime_error("Slot index must be >= 1");
                        } else {
                            m_slot_count = std::max(m_slot_count, slot_id);
                            return SlotDirective::slot(slot_id - 1);
                        }
                    } else {
                        throw std::runtime_error("Slot index must be integral");
                    }
                }
                break;

            case SymbolFunction:
                if (expr->size() == 1) {
                    // do not replace Slots in nested Functions
                    return SlotDirective::copy();
                }
                // fallthrough

            default:
                return SlotDirective::descend();
        }
    }
};

class InvalidVariable : public std::runtime_error {
public:
    InvalidVariable() : std::runtime_error("invalid var") {
    }
};

class ListArguments {
private:
    VariableMap<size_t> m_arguments;

public:
    inline void add(const BaseExpressionRef &var, index_t slot) {
        if (var->type() != SymbolType) {
            throw InvalidVariable();
        }

        m_arguments[static_cast<const Symbol*>(var.get())] = slot;
    }

    inline SlotDirective operator()(const BaseExpressionRef &item) {
        if (item->type() == SymbolType) {
            const auto i = m_arguments.find(
                static_cast<const Symbol*>(item.get()));

            if (i != m_arguments.end()) {
                return SlotDirective::slot(i->second);
            } else {
                return SlotDirective::copy();
            }
        } else {
            return SlotDirective::descend();
        }
    }
};

UnsafeSlotFunctionRef SlotFunction::construct(const Expression *body) {
    SlotArguments arguments;
    return UnsafeSlotFunctionRef(new SlotFunction(
        new RewriteExpression(arguments, body), arguments.slot_count()));
}

template<typename Arguments>
inline BaseExpressionRef SlotFunction::rewrite_or_copy(
    const Expression *body,
    const Arguments &args,
    size_t n_args) const {

    if (n_args != m_slot_count) {
        throw std::runtime_error("wrong slot count");
    }

    return m_rewrite->rewrite_or_copy(body, args);
}

class FunctionRule : public Rule {
private:
    inline BaseExpressionRef slot(
        const Expression *args,
        const BaseExpressionRef &body) const {

        if (body->type() != ExpressionType) {
            return BaseExpressionRef();
        }

        const CacheRef cache = args->ensure_cache();

        ConstSlotFunctionRef slot_function = cache->slot_function.ensure([&body] () {
            return SlotFunction::construct(body->as_expression());
        });

        return args->with_leaves_array(
            [&slot_function, &body] (const BaseExpressionRef *args, size_t n_args) {
                return slot_function->rewrite_or_copy(
                    body->as_expression(), [&args] (index_t i, const BaseExpressionRef&) {
		                return args[i];
                    }, n_args);
            });
    }

    inline BaseExpressionRef vars(
        const Expression *args,
        const BaseExpressionRef &vars,
        const BaseExpressionRef &body) const {

        if (vars->type() != ExpressionType) {
            return BaseExpressionRef();
        }
        if (vars->as_expression()->size() != args->size()) {
            return BaseExpressionRef(); // exit early if vars don't match
        }
        if (body->type() != ExpressionType) {
            return BaseExpressionRef();
        }

        const CacheRef cache = args->ensure_cache();

        try {
            ConstRewriteExpressionRef vars_function = cache->vars_function.ensure([&vars, &body] () {
                return vars->as_expression()->with_leaves_array(
                    [&body](const BaseExpressionRef *vars, size_t n_vars) {
                        ListArguments arguments;

                        for (size_t i = 0; i < n_vars; i++) {
                            arguments.add(vars[i], i);
                        }

                        return UnsafeRewriteExpressionRef(new RewriteExpression(
                            arguments, body->as_expression()));
                    });
            });

            return args->with_leaves_array(
                [&vars_function, &body] (const BaseExpressionRef *args, size_t n_args) {
                    return vars_function->rewrite_or_copy(
                        body->as_expression(), [&args] (index_t i, const BaseExpressionRef&) {
                            return args[i];
                        });
                });
        } catch(const InvalidVariable&) {
            return BaseExpressionRef();
        }
    }

public:
	FunctionRule(const SymbolRef &head, const Definitions &definitions) :
		Rule(function_pattern(head, definitions)) {
	}

	virtual BaseExpressionRef try_apply(const Expression *function, const Evaluation &evaluation) const {
		const BaseExpressionRef &head = function->_head;
		if (head->type() != ExpressionType) {
			return BaseExpressionRef();
		}

        const Expression * const head_expr = head->as_expression();
        switch (head_expr->size()) {
            case 1: // Function[body_][args___]
                return slot(function, head_expr->static_leaves<1>()[0]);

            case 2: { // Function[vars_, body_][args___]
                const BaseExpressionRef * const leaves = head_expr->static_leaves<2>();
                return vars(function, leaves[0], leaves[1]);
            }

            default:
                return BaseExpressionRef();
		}
	}

	virtual MatchSize leaf_match_size() const {
		return MatchSize::at_least(0);
	}
};

void Builtins::Functional::initialize() {
	add("Function",
		Attributes::HoldAll, {
			NewRule<FunctionRule>
		}
	);
}
