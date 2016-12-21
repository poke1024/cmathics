#include "functional.h"

// as Function is a very important pattern, we provide a special optimized Rule for it.

BaseExpressionRef function_pattern(
	const SymbolRef &head,
    const Definitions &definitions);

struct Directive {
    enum Action {
        Slot,
        Copy,
        Descend
    };

    const Action m_action;
    const index_t m_slot;

    inline Directive(Action action, index_t slot = 0) :
        m_action(action), m_slot(slot) {
    }

    inline static Directive slot(index_t slot) {
        return Directive(Directive::Slot, slot);
    }

    inline static Directive copy() {
        return Directive(Directive::Copy);
    }

    inline static Directive descend() {
        return Directive(Directive::Descend);
    }
};

class SlotArguments {
private:
    index_t m_slot_count;

public:
    SlotArguments() : m_slot_count(0) {
    }

    inline size_t slot_count() const {
        return m_slot_count;
    }

    Directive operator()(const BaseExpressionRef &item) {
        if (item->type() != ExpressionType) {
            return Directive::copy();
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
                            return Directive::slot(slot_id - 1);
                        }
                    } else {
                        throw std::runtime_error("Slot index must be integral");
                    }
                }
                break;

            case SymbolFunction:
                if (expr->size() == 1) {
                    // do not replace Slots in nested Functions
                    return Directive::copy();
                }
                // fallthrough

            default:
                return Directive::descend();
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
    std::unordered_map<const Symbol*, size_t> m_arguments;

public:
    inline void add(const BaseExpressionRef &var, index_t slot) {
        if (var->type() != SymbolType) {
            throw InvalidVariable();
        }

        m_arguments[static_cast<const Symbol*>(var.get())] = slot;
    }

    inline Directive operator()(const BaseExpressionRef &item) {
        if (item->type() == SymbolType) {
            const auto i = m_arguments.find(
                static_cast<const Symbol*>(item.get()));

            if (i != m_arguments.end()) {
                return Directive::slot(i->second);
            } else {
                return Directive::copy();
            }
        } else {
            return Directive::descend();
        }
    }
};

template<typename Arguments>
FunctionBody::Node::Node(
    Arguments &arguments,
    const BaseExpressionRef &expr) {

    const Directive directive = arguments(expr);

    switch (directive.m_action) {
        case Directive::Slot:
            m_slot = directive.m_slot;
            break;
        case Directive::Copy:
            m_slot = -1;
            break;
        case Directive::Descend:
            m_slot = -1;
            if (expr->type() == ExpressionType) {
                m_down = std::make_shared<FunctionBody>(
                    arguments, expr->as_expression());
            }
            break;
    }
}

template<typename Arguments>
std::vector<const FunctionBody::Node> FunctionBody::nodes(
    Arguments &arguments,
    const Expression *body_ptr) {

    return body_ptr->with_slice([&arguments] (const auto &slice) {
        std::vector<const Node> refs;
        const size_t n = slice.size();
        for (size_t i = 0; i < n; i++) {
            refs.emplace_back(Node(arguments, slice[i]));
        }
        return refs;
    });
}

template<typename Arguments>
FunctionBody::FunctionBody(
    Arguments &arguments,
	const Expression *body) :

    m_head(Node(arguments, body->head())),
    m_leaves(nodes(arguments, body)) {
}

inline BaseExpressionRef FunctionBody::instantiate(
    const Expression *body,
	const BaseExpressionRef *args) const {

	const auto replace = [&args] (const BaseExpressionRef &expr, const Node &node) {
        const index_t slot = node.slot();
        if (slot >= 0) {
            return args[slot];
        } else {
            const FunctionBodyRef &down = node.down();
            if (down) {
                return down->instantiate(expr->as_expression(), args);
            } else {
                return expr;
            }
        }
	};

    const auto &head = m_head;
    const auto &leaves = m_leaves;

    return body->with_slice<CompileToSliceType>(
        [body, &head, &leaves, &replace] (const auto &slice) {
            const auto generate = [&slice, &leaves, &replace] (auto &storage) {
                const size_t n = slice.size();
                for (size_t i = 0; i < n; i++) {
                    storage << replace(slice[i], leaves[i]);
                }
                return nothing();
            };

            nothing state;
            return ExpressionRef(expression(
                replace(body->head(), head),
                slice.create(generate, slice.size(), state)));

        });
}

SlotFunction::SlotFunction(const Expression *body) {
    SlotArguments arguments;
    m_function = std::make_shared<FunctionBody>(arguments, body);
    m_slot_count = arguments.slot_count();
}

inline BaseExpressionRef SlotFunction::instantiate(
    const Expression *body,
    const BaseExpressionRef *args,
    size_t n_args) const {

    if (n_args != m_slot_count) {
        throw std::runtime_error("wrong slot count");
    }

    return m_function->instantiate(body, args);
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
        SlotFunctionRef slot_function = cache->slot_function;

        if (!slot_function) {
            slot_function = std::make_shared<SlotFunction>(body->as_expression());
            cache->slot_function = slot_function;
        }

        return args->with_leaves_array(
            [&slot_function, &body] (const BaseExpressionRef *args, size_t n_args) {
                return slot_function->instantiate(
                    body->as_expression(), args, n_args);
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
        FunctionBodyRef vars_function = cache->vars_function;

        if (!vars_function) {
            try {
                vars_function = vars->as_expression()->with_leaves_array(
                    [&body](const BaseExpressionRef *vars, size_t n_vars) {
                        ListArguments arguments;

                        for (size_t i = 0; i < n_vars; i++) {
                            arguments.add(vars[i], i);
                        }

                        return std::make_shared<FunctionBody>(
                            arguments, body->as_expression());
                    });
            } catch(const InvalidVariable&) {
                return BaseExpressionRef();
            }

            cache->vars_function = vars_function;
        }

        return args->with_leaves_array(
            [&vars_function, &body] (const BaseExpressionRef *args, size_t n_args) {
                return vars_function->instantiate(
                    body->as_expression(), args);
            });
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
