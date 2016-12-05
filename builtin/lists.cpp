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

template<typename F>
inline bool iterate_integer_range(
    const SymbolRef &command,
    const F &f,
    const BaseExpressionRef &imin,
    const BaseExpressionRef &imax,
    const BaseExpressionRef &di,
    const Evaluation &evaluation,
    ExpressionRef &result) {

    const machine_integer_t imin_integer =
        static_cast<const class MachineInteger *>(imin.get())->value;
    const machine_integer_t imax_integer =
        static_cast<const class MachineInteger *>(imax.get())->value;
    const machine_integer_t di_integer =
        static_cast<const class MachineInteger *>(di.get())->value;

    if (imin_integer > imax_integer) {
        result = expression(evaluation.List);
        return true;
    }

    machine_integer_t range;

    if (__builtin_ssubll_overflow(imax_integer, imin_integer, &range)) {
        return false;
    }

    if (di_integer < 1) {
        evaluation.message(command, "iterb");
        return true;
    }

    machine_integer_t n_integer;

    if (__builtin_saddll_overflow(range / di_integer, 1, &n_integer)) {
        return false;
    }

    result = f(imin_integer, imax_integer, di_integer, n_integer, evaluation);
    return true;
}

inline ExpressionRef machine_integer_range(
    const machine_integer_t imin,
    const machine_integer_t imax,
    const machine_integer_t di,
    const machine_integer_t n,
    const Evaluation &evaluation) {

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

template<typename F>
class machine_integer_table {
private:
    const F &m_func;

public:
    inline machine_integer_table(const F &func) : m_func(func) {
    }

    inline ExpressionRef operator()(
        const machine_integer_t imin,
        const machine_integer_t imax,
        const machine_integer_t di,
        const machine_integer_t n,
        const Evaluation &evaluation) const {

        std::vector<BaseExpressionRef> result;
        result.reserve(n);
        TypeMask type_mask = 0;

        machine_integer_t index = imin;
        while (index <= imax) {
            BaseExpressionRef leaf = m_func(Heap::MachineInteger(index));
            type_mask |= leaf->base_type_mask();
            result.push_back(std::move(leaf));
            index += di;
        }

        return expression(evaluation.List, std::move(result), type_mask);
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
            ExpressionRef result;
            if (iterate_integer_range(m_symbol, machine_integer_range, imin, imax, di, evaluation, result)) {
                return result;
            }
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

class IterationFunction {
protected:
    const SymbolRef m_symbol;

    template<typename F>
    ExpressionRef iterate(
        const F &f,
        const BaseExpressionRef &imin,
        const BaseExpressionRef &imax,
        const BaseExpressionRef &di,
        const Evaluation &evaluation) const {

        if (imin->type() == MachineIntegerType &&
            imax->type() == MachineIntegerType &&
            di->type() == MachineIntegerType) {

            ExpressionRef result;
            if (iterate_integer_range(m_symbol, machine_integer_table<F>(f), imin, imax, di, evaluation, result)) {
                return result;
            }
        }

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

            BaseExpressionRef leaf = f(index);
            type_mask |= leaf->base_type_mask();
            result.push_back(std::move(leaf));

            index = expression(Plus, index, di)->evaluate_or_copy(evaluation);
        }

        return expression(evaluation.List, std::move(result), type_mask);
    }

public:
    IterationFunction(const SymbolRef &symbol) : m_symbol(symbol) {
    }
};

class Table : public IterationFunction {
public:
	using IterationFunction::IterationFunction;

    template<typename F>
    inline ExpressionRef apply(
        const F &f,
        const BaseExpressionRef &iter,
        const Evaluation &evaluation) const {

        const Expression *list = if_list(iter);
        if (!list) {
            evaluation.message(m_symbol, "iterb");
            return ExpressionRef();
        }

	    const Table * const self = this;
	    return list->with_slice<CompileToSliceType>([self, &f, &evaluation] (const auto &slice) {
		    switch (slice.size()) {
			    case 1: { // {imax_}
				    return self->iterate(
					    [&f] (const BaseExpressionRef &) {
                            return f();
                        },
                        Heap::MachineInteger(0),
                        slice[0]->evaluate_or_copy(evaluation),
                        Heap::MachineInteger(1),
                        evaluation);
			    }

			    case 2: { // {i_Symbol, imax_} or {i_Symbol, {items___}}
				    const BaseExpressionRef &iterator_expr = slice[0];

				    if (iterator_expr->type() == SymbolType) {
					    Symbol * const iterator = iterator_expr->as_symbol();

					    const BaseExpressionRef &domain = slice[1];
					    const Expression *domain_list = if_list(domain);

					    if (domain_list) {
						    domain_list = if_list(domain_list->evaluate_or_copy(evaluation));
						    if (domain_list) {
							    return domain_list->with_slice<CompileToSliceType>(
								    [iterator, &f, &evaluation] (const auto &slice) {
									    return expression(
										    evaluation.List,
										    [iterator, &slice, &f, &evaluation] (auto &storage) {
											    const size_t n = slice.size();

											    for (size_t i = 0; i < n; i++) {
												    storage << scope(
													    iterator,
													    slice[i],
													    f);
											    }
										    },
										    slice.size()
									    );
								    });
						    }
					    } else {
						    return self->iterate(
                                scoped(iterator->as_symbol(), f),
							    Heap::MachineInteger(1),
							    slice[1]->evaluate_or_copy(evaluation),
							    Heap::MachineInteger(1),
							    evaluation);
					    }
				    }
				    break;
			    }
			    case 3: { // {i_Symbol, imin_, imax_}
				    const BaseExpressionRef &iterator = slice[0];

				    if (iterator->type() == SymbolType) {
					    return self->iterate(
                            scoped(iterator->as_symbol(), f),
						    slice[1]->evaluate_or_copy(evaluation),
						    slice[2]->evaluate_or_copy(evaluation),
						    Heap::MachineInteger(1),
						    evaluation);
				    }
				    break;
			    }
			    case 4: { // {i_Symbol, imin_, imax_, di_}
				    const BaseExpressionRef &iterator = slice[0];

				    if (iterator->type() == SymbolType) {
					    return self->iterate(
                            scoped(iterator->as_symbol(), f),
						    slice[1]->evaluate_or_copy(evaluation),
						    slice[2]->evaluate_or_copy(evaluation),
						    slice[3]->evaluate_or_copy(evaluation),
						    evaluation);
				    }
				    break;
			    }

                default:
                    evaluation.message(self->m_symbol, "iterb");
                    break;
            }

		    return ExpressionRef();
	    });
    }
};

template<typename F>
class IterationFunctionRule : public AtLeastNRule<2> {
private:
    const SymbolRef m_head;
    const F m_function;

    ExpressionRef apply(
        const BaseExpressionRef &expr,
        const BaseExpressionRef *iter_begin,
        const BaseExpressionRef *iter_end,
        const Evaluation &evaluation) const {

        if (iter_end - iter_begin == 1) {
            return m_function.apply([&expr, &evaluation] () {
                return expr->evaluate_or_copy(evaluation);
            }, iter_begin[0], evaluation);
        } else {
            const auto self = this;

            return m_function.apply([self, &expr, iter_begin, iter_end, &evaluation] () {
                return self->apply(expr, iter_begin + 1, iter_end, evaluation);
            }, iter_begin[0], evaluation);
        }
    }

public:
    IterationFunctionRule(const SymbolRef &head, const Definitions &definitions) :
        AtLeastNRule<2>(head, definitions), m_head(head), m_function(head) {
    }

    virtual BaseExpressionRef try_apply(const Expression *expr, const Evaluation &evaluation) const {
        const auto self = this;

        auto result = expr->with_leaves_array([self, &evaluation] (const BaseExpressionRef *leaves, size_t n) {
            return self->apply(leaves[0], leaves + 1, leaves + n, evaluation);
        });

		return result;
    }
};

template<typename F>
NewRuleRef make_iteration_function_rule() {
    return [] (const SymbolRef &head, const Definitions &definitions) {
        return std::make_shared<IterationFunctionRule<F>>(head, definitions);
    };
}

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
					return expr->as_expression()->with_slice<CompileToSliceType>([&f, &expr] (const auto &slice) {

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

    add("Table",
        Attributes::HoldAll, {
            make_iteration_function_rule<Table>()
        });
}
