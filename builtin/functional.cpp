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
        if (!item->is_expression()) {
            return SlotDirective::copy();
        }

        const Expression *expr = item->as_expression();
        const BaseExpressionRef &head = expr->head();

        switch (head->symbol()) {
            case S::Slot:
                if (expr->size() != 1) {
                    // throw std::runtime_error("Slot must contain one leaf");
                    return SlotDirective::copy();
                } else {
                    const BaseExpressionRef *leaves = expr->n_leaves<1>();
                    const BaseExpressionRef &slot = leaves[0];

                    if (slot->is_machine_integer()) {
                        const machine_integer_t slot_id =
                            static_cast<const MachineInteger *>(slot.get())->value;
                        if (slot_id < 1) {
                            return SlotDirective::illegal_slot(slot);
                        } else {
                            m_slot_count = std::max(m_slot_count, slot_id);
                            return SlotDirective::slot(slot_id - 1);
                        }
                    } else {
                        return SlotDirective::illegal_slot(slot);
                    }
                }
                break;

            case S::OptionValue:
                if (expr->size() != 1) {
                    return SlotDirective::copy();
                } else {
                    const BaseExpressionRef *leaves = expr->n_leaves<1>();
                    const BaseExpressionRef &option = leaves[0];

                    if (option->is_symbol()) {
                        return SlotDirective::option_value(option->as_symbol());
                    } else {
                        return SlotDirective::copy();
                    }
                }
                break;

            case S::Function:
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
    SymbolPtrMap<size_t> m_arguments;

public:
    inline void add(const BaseExpressionRef &var, index_t slot) {
        if (!var->is_symbol()) {
            throw InvalidVariable();
        }

        m_arguments[static_cast<const Symbol*>(var.get())] = slot;
    }

    inline SlotDirective operator()(const BaseExpressionRef &item) {
        if (item->is_symbol()) {
            const auto i = m_arguments.find(item->as_symbol());

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

UnsafeSlotFunctionRef SlotFunction::from_expression(const Expression *body, Definitions &definitions) {
    SlotArguments arguments;
    return UnsafeSlotFunctionRef(SlotFunction::construct(
        Rewrite::from_arguments(arguments, body, definitions), arguments.slot_count()));
}

template<typename Arguments>
inline BaseExpressionRef SlotFunction::rewrite_or_copy(
    const Expression *body,
    const Arguments &args,
    size_t n_args,
    const Evaluation &evaluation) const {

    const auto no_symbols = [] (const SymbolRef &name) {
        return BaseExpressionRef();
    };

    if (n_args >= m_slot_count) {
        return m_rewrite->rewrite_or_copy(
            body, args, no_symbols, evaluation);
    } else {
        return m_rewrite->rewrite_or_copy(
            body,
            [&args, &evaluation, n_args] (index_t i, const BaseExpressionRef &expr) {
                if (size_t(i) >= n_args) {
                    evaluation.message(evaluation.Function, "slotn", MachineInteger::construct(1 + i));
                    return expr;
                } else {
                    return args(i, expr);
                }
            },
            no_symbols,
            evaluation);
    }
}

class FunctionRule : public Rule, public ExtendedHeapObject<FunctionRule> {
private:
    inline BaseExpressionRef slot(
        const Expression *args,
        const BaseExpressionRef &body,
        const Evaluation &evaluation) const {

        if (!body->is_expression()) {
            return BaseExpressionRef();
        }

        const CacheRef cache = body->as_expression()->ensure_cache();

        ConstSlotFunctionRef slot_function = cache->slot_function.ensure([&body, &evaluation] () {
            return SlotFunction::from_expression(body->as_expression(), evaluation.definitions);
        });

        return args->with_leaves_array(
            [&slot_function, &body, &evaluation] (const BaseExpressionRef *args, size_t n_args) {
                return slot_function->rewrite_or_copy(
                    body->as_expression(), [&args] (index_t i, const BaseExpressionRef&) {
		                return args[i];
                    }, n_args, evaluation);
            });
    }

    inline BaseExpressionRef vars(
        const Expression *args,
        const BaseExpressionRef &vars,
        const BaseExpressionRef &body,
        const Evaluation &evaluation) const {

        size_t n_vars;

        if (vars->is_expression()) {
            n_vars = vars->as_expression()->size();
        } else if (vars->is_symbol()) {
            n_vars = 1;
        } else {
            return BaseExpressionRef();
        }

        if (n_vars > args->size()) {
            evaluation.message(evaluation.Function, "fpct");
            return BaseExpressionRef();
        }

        if (!body->is_expression()) {
            return BaseExpressionRef();
        }

        const CacheRef cache = body->as_expression()->ensure_cache();

        try {
            ConstRewriteExpressionRef vars_function =
                cache->vars_function.ensure([&vars, &body, &evaluation] () {
                    ListArguments arguments;

                    if (vars->is_expression()) {
                        vars->as_expression()->with_leaves_array(
                            [&arguments] (const BaseExpressionRef *vars, size_t n_vars) {

                            for (size_t i = 0; i < n_vars; i++) {
                                arguments.add(vars[i], i);
                            }
                        });
                    } else {
                        assert(vars->is_symbol());
                        arguments.add(vars->as_symbol(), 0);
                    }

                    return UnsafeRewriteExpressionRef(RewriteExpression::from_arguments(
                        arguments, body->as_expression(), evaluation.definitions));
                });

            return args->with_leaves_array(
                [&vars_function, &body, &evaluation] (const BaseExpressionRef *args, size_t n_args) {
                    return vars_function->rewrite_or_copy(
                        [&args] (index_t i, const BaseExpressionRef&) {
                            return args[i];
                        }, [] (const SymbolRef &option) {
                            return BaseExpressionRef();
                        }, evaluation);
                });
        } catch(const InvalidVariable&) {
            return BaseExpressionRef();
        }
    }

public:
	FunctionRule(const SymbolRef &head, const Definitions &definitions) :
		Rule(function_pattern(head, definitions)) {
	}

	virtual optional<BaseExpressionRef> try_apply(
        const Expression *function,
        const Evaluation &evaluation) const {

		const BaseExpressionRef &head = function->_head;
		if (!head->is_expression()) {
			return optional<BaseExpressionRef>();
		}

        const Expression * const head_expr = head->as_expression();
        switch (head_expr->size()) {
            case 1: // Function[body_][args___]
                return slot(function, head_expr->n_leaves<1>()[0], evaluation);

            case 2: { // Function[vars_, body_][args___]
                const BaseExpressionRef * const leaves = head_expr->n_leaves<2>();
                return vars(function, leaves[0], leaves[1], evaluation);
            }

            default:
                return optional<BaseExpressionRef>();
		}
	}

	virtual MatchSize leaf_match_size() const {
		return MatchSize::at_least(0);
	}
};

class Function : public Builtin {
public:
    static constexpr const char *name = "Function";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Function[$body$]'
    <dt>'$body$ &'
        <dd>represents a pure function with parameters '#1', '#2', etc.
    <dt>'Function[{$x1$, $x2$, ...}, $body$]'
        <dd>represents a pure function with parameters $x1$, $x2$, etc.
    </dl>

    >> f := # ^ 2 &
    >> f[3]
     = 9
    >> #^3& /@ {1, 2, 3}
     = {1, 8, 27}
    >> #1+#2&[4, 5]
     = 9

    You can use 'Function' with named parameters:
    >> Function[{x, y}, x * y][2, 3]
     = 6

    Parameters are renamed, when necessary, to avoid confusion:
    >> Function[{x}, Function[{y}, f[x, y]]][y]
     = Function[{y$}, f[y, y$]]
    >> Function[{y}, f[x, y]] /. x->y
     = Function[{y}, f[y, y]]
    >> Function[y, Function[x, y^x]][x][y]
     = x ^ y
    >> Function[x, Function[y, x^y]][x][y]
     = x ^ y

    Slots in inner functions are not affected by outer function application:
    >> g[#] & [h[#]] & [5]
     = g[h[5]]

    >> g[x_,y_] := x+y
    >> g[Sequence@@Slot/@Range[2]]&[1,2]
     = #1 + #2
    >> Evaluate[g[Sequence@@Slot/@Range[2]]]&[1,2]
     = 3
	)";

    static constexpr auto attributes = Attributes::HoldAll;

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        message("slot", "`1` should contain a positive integer.");
        message("slotn", "Slot number `1` cannot be filled.");
        message("fpct", "Too many parameters to be filled.");

        builtin<FunctionRule>();
    }
};

class SlotBuiltin : public Builtin {
public:
	static constexpr const char *name = "Slot";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'#$n$'
        <dd>represents the $n$th argument to a pure function.
    <dt>'#'
        <dd>is short-hand for '#1'.
    <dt>'#0'
        <dd>represents the pure function itself.
    </dl>

    >> #
     = #1

    Unused arguments are simply ignored:
    >> {#1, #2, #3}&[1, 2, 3, 4, 5]
     = {1, 2, 3}
	)";

	static constexpr auto attributes = Attributes::NHoldAll;

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("Slot[]", "Slot[1]");
		builtin("MakeBoxes[Slot[n_Integer?NonNegative], f:StandardForm|TraditionalForm|InputForm|OutputForm]",
			"\"#\" <> ToString[n]");
	}
};

void Builtins::Functional::initialize() {
    add<Function>();
	add<SlotBuiltin>();
}
