#include "lists.h"
#include "../core/definitions.h"

class First : public Builtin {
public:
    static constexpr const char *name = "First";

public:
    using Builtin::Builtin;

    void build() {
        message("nofirst", "There is no first element in `1`.");

        builtin(&First::apply);
    }

    inline BaseExpressionRef apply(const BaseExpressionRef &x, const Evaluation &evaluation) {
        if (x->type() != ExpressionType) {
            evaluation.message(m_symbol, "normal");
        } else {
            const Expression *expr = x->as_expression();
            if (expr->size() < 1) {
                evaluation.message(m_symbol, "nofirst", x);
            } else {
	            return expr->leaf(0);
            }
        }
        return BaseExpressionRef();
    }
};

class Last : public Builtin {
public:
    static constexpr const char *name = "Last";

public:
    using Builtin::Builtin;

    void build() {
        message("nolast", "There is no last element in `1`.");

        builtin(&Last::apply);
    }

	inline BaseExpressionRef apply(const BaseExpressionRef &x, const Evaluation &evaluation) {
        if (x->type() != ExpressionType) {
            evaluation.message(m_symbol, "normal");
        } else {
            const Expression *expr = x->as_expression();
            const size_t size = expr->size();
            if (size < 1) {
                evaluation.message(m_symbol, "nolast", x);
            } else {
	            return expr->leaf(size - 1);
            }
        }
        return BaseExpressionRef();
    }
};

class Most : public Builtin {
public:
    static constexpr const char *name = "Most";

public:
    using Builtin::Builtin;

    void build() {
        message("nomost", "Most is not applicable to `1`.");

        builtin(&Most::apply);
    }

	inline BaseExpressionRef apply(const BaseExpressionRef &x, const Evaluation &evaluation) {
        if (x->type() != ExpressionType) {
            evaluation.message(m_symbol, "normal");
        } else {
            const Expression *expr = x->as_expression();
            if (expr->size() < 1) {
                evaluation.message(m_symbol, "nomost", expr);
            } else {
                return expr->slice(0, -1);
            }
        }
        return BaseExpressionRef();
    }
};

class Rest : public Builtin {
public:
    static constexpr const char *name = "Rest";

public:
    using Builtin::Builtin;

    void build() {
        message("norest", "Rest is not applicable to `1`.");

        builtin(&Rest::apply);
    }

	inline BaseExpressionRef apply(const BaseExpressionRef &x, const Evaluation &evaluation) {
        if (x->type() != ExpressionType) {
            evaluation.message(m_symbol, "normal");
        } else {
            const Expression *expr = x->as_expression();
            if (expr->size() < 1) {
                evaluation.message(m_symbol, "norest", expr);
            } else {
                return expr->slice(1);
            }
        }
        return BaseExpressionRef();
    }
};

class Select : public Builtin {
public:
    static constexpr const char *name = "Select";

public:
    using Builtin::Builtin;

    void build() {
        builtin(&Select::apply);
    }

	inline BaseExpressionRef apply(
        const BaseExpressionRef &list,
        const BaseExpressionRef &cond,
        const Evaluation &evaluation) {

        if (list->type() != ExpressionType) {
            evaluation.message(m_symbol, "normal");
        } else {
	        return list->as_expression()->with_slice(
			    [list, &cond, &evaluation] (const auto &slice) {

		            const size_t size = slice.size();

			        std::vector<BaseExpressionRef> remaining;
			        remaining.reserve(size);

			        for (size_t i = 0; i < size; i++) {
				        const auto leaf = slice[i];
				        if (expression(cond, leaf)->evaluate(evaluation)->is_true()) {
					        remaining.push_back(leaf);
				        }
			        }

			        return expression(list->as_expression()->head(), std::move(remaining));
	           }
	        );
        }
        return BaseExpressionRef();
    }
};

class Range : public Builtin {
public:
	static constexpr const char *name = "Range";

public:
	using Builtin::Builtin;

	void build() {
		rewrite("Range[imax_]", "Range[1, imax, 1]");
		rewrite("Range[imin_, imax_]", "Range[imin, imax, 1]");
		builtin(&Range::apply);
	}

protected:
	BaseExpressionRef MachineInteger(
		const BaseExpressionRef &imin_expr,
		const BaseExpressionRef &imax_expr,
		const BaseExpressionRef &di_expr,
		const Evaluation &evaluation) {

		const machine_integer_t imin = static_cast<const class MachineInteger*>(imin_expr.get())->value;
		const machine_integer_t imax = static_cast<const class MachineInteger*>(imax_expr.get())->value;
		const machine_integer_t di = static_cast<const class MachineInteger*>(di_expr.get())->value;

		if (imin > imax) {
			return expression(evaluation.List);
		}

		machine_integer_t range;

		if (__builtin_ssubll_overflow(imax, imin, &range)) {
            evaluation.message(m_symbol, "iterb");
			return BaseExpressionRef(); // error; too large
		}

		if (di < 1) {
            evaluation.message(m_symbol, "iterb");
			return BaseExpressionRef(); // error
		}

		machine_integer_t n;

		if (__builtin_saddll_overflow(range / di, 1, &n)) {
            evaluation.message(m_symbol, "iterb");
			return BaseExpressionRef(); // error; too large
		}

		if (n >= MinPackedSliceSize) {
			std::vector<machine_integer_t> leaves;
			leaves.reserve(n);
			for (machine_integer_t x = imin; x <= imax; x += di) {
				leaves.push_back(x);
			}
			return expression(
				evaluation.List,
				PackedSlice<machine_integer_t>(std::move(leaves)));
		} else {
			return expression(
				evaluation.List,
				[imin, imax, di] (auto &storage) {
					for (machine_integer_t x = imin; x <= imax; x += di) {
						storage << Heap::MachineInteger(x);
					}
				},
				n
			);
		}
	}

	BaseExpressionRef MachineReal(
		const BaseExpressionRef &imin_expr,
		const BaseExpressionRef &imax_expr,
		const BaseExpressionRef &di_expr,
		const Evaluation &evaluation) {

		const machine_real_t imin = imin_expr->round_to_float();
		const machine_real_t imax = imax_expr->round_to_float();
		const machine_real_t di = di_expr->round_to_float();

		if (imin > imax) {
			return expression(evaluation.List);
		}

		if (di <= 0.) {
            evaluation.message(m_symbol, "iterb");
			return BaseExpressionRef(); // error
		}

		std::vector<machine_real_t> leaves;
		leaves.reserve(size_t(std::ceil((imax - imin) / di)));
		for (machine_real_t x = imin; x <= imax; x += di) {
			leaves.push_back(x);
		}

		if (leaves.size() >= MinPackedSliceSize) {
			return expression(
				evaluation.List,
				PackedSlice<machine_real_t>(std::move(leaves)));
		} else {
			return expression(
				evaluation.List,
				[&leaves] (auto &storage) {
					for (machine_real_t x : leaves) {
						storage << Heap::MachineReal(x);
					}
				},
				leaves.size()
			);
		}
	}

	BaseExpressionRef generic(
		const BaseExpressionRef &imin,
		const BaseExpressionRef &imax,
		const BaseExpressionRef &di,
		const Evaluation &evaluation) {

		const BaseExpressionRef &LessEqual = evaluation.LessEqual;
		const BaseExpressionRef &Plus = evaluation.Plus;

		std::vector<BaseExpressionRef> result;
		TypeMask type_mask = 0;

		BaseExpressionRef index = imin;
		while (true) {
			const ExtendedType if_continue =
				expression(LessEqual, index, imax)->
					evaluate_or_copy(evaluation)->extended_type();

			if (if_continue == SymbolFalse) {
				break;
			} else if (if_continue != SymbolTrue) {
				evaluation.message(m_symbol, "iterb");
				return BaseExpressionRef();
			}

			type_mask |= index->base_type_mask();
			result.push_back(index);

			index = expression(Plus, index, di)->evaluate_or_copy(evaluation);
		}

		return expression(evaluation.List, std::move(result), type_mask);
	}

public:
	BaseExpressionRef apply(
		const BaseExpressionRef &imin,
		const BaseExpressionRef &imax,
		const BaseExpressionRef &di,
		const Evaluation &evaluation) {

		const TypeMask type_mask =
			imin->base_type_mask() |
			imax->base_type_mask() |
			di->base_type_mask();

		if (type_mask & MakeTypeMask(MachineRealType)) { // expression contains a MachineReal
			return MachineReal(imin, imax, di, evaluation);
		}

		constexpr TypeMask machine_int_mask = MakeTypeMask(MachineIntegerType);

		if ((type_mask & machine_int_mask) == type_mask) { // expression is all MachineIntegers
			return MachineInteger(imin, imax, di, evaluation);
		}

		return generic(imin, imax, di, evaluation);
	}
};

inline const Expression *if_list(const BaseExpressionRef &item) {
    if (item->type() == ExpressionType) {
        const Expression *expr =item->as_expression();
        if (expr->head()->extended_type() == SymbolList) {
            return expr;
        }
    }
    return nullptr;
}

class IterationFunction : public Builtin {
protected:
    template<bool SYMBOL>
    ExpressionRef iterate(
        const BaseExpressionRef &expr,
        Symbol *iterator,
        const BaseExpressionRef &imin,
        const BaseExpressionRef &imax,
        const BaseExpressionRef &di,
        const Evaluation &evaluation) {

        const BaseExpressionRef &LessEqual = evaluation.LessEqual;
        const BaseExpressionRef &Plus = evaluation.Plus;

        std::vector<BaseExpressionRef> result;
        TypeMask type_mask = 0;

        BaseExpressionRef index = imin;
        while (true) {
            const ExtendedType if_continue =
                expression(LessEqual, index, imax)->
                    evaluate_or_copy(evaluation)->extended_type();

            if (if_continue == SymbolFalse) {
                break;
            } else if (if_continue != SymbolTrue) {
                evaluation.message(m_symbol, "iterb");
                return ExpressionRef();
            }

            BaseExpressionRef leaf;
            if (SYMBOL) {
                leaf = scope(iterator, index, [&expr, &evaluation]() {
                    return expr->evaluate_or_copy(evaluation);
                });
            } else {
                leaf = expr->evaluate_or_copy(evaluation);
            }
            type_mask |= leaf->base_type_mask();
            result.push_back(std::move(leaf));

            index = expression(Plus, index, di)->evaluate_or_copy(evaluation);
        }

        return expression(evaluation.List, std::move(result), type_mask);
    }

public:
    using Builtin::Builtin;
};

class Table : public IterationFunction {
public:
    static constexpr const char *name = "Table";

public:
    using IterationFunction::IterationFunction;

    void build() {
        builtin(&Table::apply);
    }

    BaseExpressionRef apply(
        const BaseExpressionRef &expr,
        const BaseExpressionRef &iter,
        const Evaluation &evaluation) {

        const Expression *list = if_list(iter);
        if (!list) {
            return BaseExpressionRef();
        }

        switch (list->size()) {
            case 1: { // {imax_}
                const BaseExpressionRef *leaves = list->static_leaves<1>();
                return iterate<false>(
                    expr,
                    nullptr,
                    Heap::MachineInteger(0),
                    leaves[0]->evaluate_or_copy(evaluation),
                    Heap::MachineInteger(1),
                    evaluation);
            }

            case 2: { // {i_Symbol, imax_} or {i_Symbol, {items___}}
                const BaseExpressionRef *leaves = list->static_leaves<2>();
                const BaseExpressionRef &iterator = leaves[0];

                if (iterator->type() == SymbolType) {
                    const BaseExpressionRef &domain = leaves[1];
                    const Expression *domain_list = if_list(domain);

                    if (domain_list) {
                        domain_list = if_list(domain_list->evaluate_or_copy(evaluation));
                        if (domain_list) {
                            return domain_list->iterate(iterator->as_symbol(), expr, evaluation);
                        }
                    } else {
                        return iterate<true>(
                            expr,
                            iterator->as_symbol(),
                            Heap::MachineInteger(1),
                            leaves[1]->evaluate_or_copy(evaluation),
                            Heap::MachineInteger(1),
                            evaluation);
                    }
                }
                break;
            }
            case 3: { // {i_Symbol, imin_, imax_}
                const BaseExpressionRef *leaves = list->static_leaves<3>();
                const BaseExpressionRef &iterator = leaves[0];

                if (iterator->type() == SymbolType) {
                    return iterate<true>(
                        expr,
                        iterator->as_symbol(),
                        leaves[1]->evaluate_or_copy(evaluation),
                        leaves[2]->evaluate_or_copy(evaluation),
                        Heap::MachineInteger(1),
                        evaluation);
                }
                break;
            }
            case 4: { // {i_Symbol, imin_, imax_, di_}
                const BaseExpressionRef *leaves = list->static_leaves<4>();
                const BaseExpressionRef &iterator = leaves[0];

                if (iterator->type() == SymbolType) {
                    return iterate<true>(
                        expr,
                        iterator->as_symbol(),
                        leaves[1]->evaluate_or_copy(evaluation),
                        leaves[2]->evaluate_or_copy(evaluation),
                        leaves[3]->evaluate_or_copy(evaluation),
                         evaluation);
                }
                break;
            }
        }

        return BaseExpressionRef();
    }
};

void Builtins::Lists::initialize() {

    add("ListQ",
        Attributes::None, {
            builtin<1>([](const BaseExpressionRef &x, const Evaluation &evaluation) {
                if (x->type() == ExpressionType) {
                    return evaluation.Boolean(x->as_expression()->_head == evaluation.List);
                } else {
                    return evaluation.False;
                }
            })
        });

    add("NotListQ",
        Attributes::None, {
            builtin<1>([](const BaseExpressionRef &x, const Evaluation &evaluation) {
                if (x->type() == ExpressionType) {
                    return evaluation.Boolean(x->as_expression()->_head != evaluation.List);
                } else {
                    return evaluation.True;
                }
            })
        });

    add("Length",
        Attributes::None, {
             builtin<1>(
                 [](const BaseExpressionRef &x, const Evaluation &evaluation) {
                     if (x->type() != ExpressionType) {
                        return from_primitive(machine_integer_t(0));
                     } else {
                         const Expression *list = x->as_expression();
                         return from_primitive(machine_integer_t(list->size()));
                     }
                 }
             )
        });

    add("Apply",
	    Attributes::None, {
			builtin<2>(
				[](const BaseExpressionRef &f, const BaseExpressionRef &x, const Evaluation &evaluation) {
				    if (x->type() != ExpressionType) {
					    throw std::runtime_error("expected Expression at position 2");
				    }
				    return x->clone(f);
			    }
		    )
	    });

	add<First>();
    add<Last>();
    add<Most>();
    add<Rest>();

    add<Select>();

	add("Map",
	    Attributes::None, {
			builtin<2>(
				[](const BaseExpressionRef &f, const BaseExpressionRef &expr, const Evaluation &evaluation) {
				    if (expr->type() != ExpressionType) {
					    return ExpressionRef();
				    }
					return expr->as_expression()->with_slice<OptimizeForSpeed>([&f, &expr] (const auto &slice) {

						return expression(expr->as_expression()->head(), [&f, &slice] (auto &storage) {
							const size_t size = slice.size();

							for (size_t i = 0; i < size; i++) {
								const auto leaf = slice[i];
								storage << expression(f, StaticSlice<1>(&leaf, leaf->base_type_mask()));
							}
						}, slice.size());
					});
			    }
		    )
	    });

	add<Range>();

	add("Mean",
	    Attributes::None, {
			rewrite("Mean[x_List]", "Total[x] / Length[x]"),
	    });

	add("Total",
	    Attributes::None, {
			rewrite("Total[head_]", "Apply[Plus, head]"),
			rewrite("Total[head_, n_]", "Apply[Plus, Flatten[head, n]]"),
	    });

    add<Table>();
}
