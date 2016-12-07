#include "lists.h"
#include "../core/definitions.h"

inline const Expression *if_list(const BaseExpressionRef &item) {
    if (item->type() == ExpressionType) {
        const Expression *expr =item->as_expression();
        if (expr->head()->extended_type() == SymbolList) {
            return expr;
        }
    }
    return nullptr;
}

class invalid_levelspec_error : public std::runtime_error {
public:
    invalid_levelspec_error() : std::runtime_error("invalid levelspec") {
    }
};

inline bool is_infinity(const Expression *expr) {
    if (expr->head()->extended_type() != SymbolDirectedInfinity) {
        return false;
    }
    if (expr->size() != 1) {
        return false;
    }
    if (!expr->static_leaves<1>()[0]->is_one()) {
        return false;
    }
    return true;
}

class Levelspec {
public:
    struct Position {
        const Position *up;
        size_t index;
    };

private:
    optional<machine_integer_t> m_start;
    optional<machine_integer_t> m_stop;

    static optional<machine_integer_t> value_to_level(const BaseExpressionRef &item) {
        switch (item->type()) {
            case MachineIntegerType:
                return static_cast<const MachineInteger*>(item.get())->value;
            case ExpressionType:
                if (is_infinity(item->as_expression())) {
                    return optional<machine_integer_t>();
                } else {
                    throw invalid_levelspec_error();
                }
            default:
                throw invalid_levelspec_error();
        }
    }

public:
    Levelspec(BaseExpressionPtr spec) {
        const Expression *list = if_list(spec);
        if (list) {
            switch (list->size()) {
                case 1: {
                    const BaseExpressionRef *leaves = list->static_leaves<1>();
                    const optional<machine_integer_t> i = value_to_level(leaves[0]);
                    m_start = i;
                    m_stop = i;
                    break;
                }
                case 2: {
                    const BaseExpressionRef *leaves = list->static_leaves<2>();
                    m_start = value_to_level(leaves[0]);
                    m_stop = value_to_level(leaves[1]);
                    break;
                }
                default:
                    throw invalid_levelspec_error();
            }
        } else if (spec->extended_type() == SymbolAll) {
            m_start = 0;
            m_stop = optional<machine_integer_t>();
        } else {
            m_start = 1;
            m_stop = value_to_level(spec);
        }
    }

    bool is_in_level(index_t current, index_t depth) const {
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

	enum WalkMode {
		Select,
		Mutable
	};

    template<WalkMode Mode, typename Callback>
    std::tuple<BaseExpressionRef, size_t> walk(
        const BaseExpressionRef &node,
        const Callback &callback,
        index_t current = 0,
        const Position *pos = nullptr) const {

        size_t depth = 0;
        BaseExpressionRef new_node;

        if (node->type() == ExpressionType) {
            const Expression * const expr =
                static_cast<const Expression*>(node.get());

            BaseExpressionRef head = expr->head();

            Position new_pos;
            new_pos.up = pos;

            const Levelspec *self = this;

            std::tie(new_node, depth) = expr->with_slice<Mode == Select ? DoNotCompileToSliceType : CompileToSliceType>(
                [self, current, &callback, &head, &new_pos, &depth]
                (const auto &slice)  {

                    return transform(head, slice, 0, slice.size(),
                        [self, current, &callback, &new_pos, &depth]
                        (size_t index0, const BaseExpressionRef &leaf, size_t depth)  {

                            BaseExpressionRef walk_leaf;
                            size_t walk_leaf_depth;

                            new_pos.index = index0;

                            std::tie(walk_leaf, walk_leaf_depth) = self->walk<Mode>(
                                leaf, callback, current + 1, &new_pos);

                            if (walk_leaf_depth + 1 > depth) {
                                depth = walk_leaf_depth + 1;
                            }

                            return std::make_tuple(walk_leaf, depth);

                    }, depth, false);

            });
        }

        if (is_in_level(current, depth)) {
            return std::make_tuple(callback(new_node ? new_node : node, pos), depth);
        } else {
            return std::make_tuple(new_node, depth);
        }
    }
};

class Level : public Builtin {
public:
    static constexpr const char *name = "Level";

public:
    using Builtin::Builtin;

    void build() {
        builtin(&Level::apply);
    }

    inline BaseExpressionRef apply(BaseExpressionPtr expr, BaseExpressionPtr ls, const Evaluation &evaluation) {
        Levelspec levelspec(ls);
        return expression_from_generator(evaluation.List, [&expr, &levelspec] (auto &storage) {
            levelspec.walk<Levelspec::Select>(BaseExpressionRef(expr), [&storage] (const auto &node, const auto *pos) {
                storage << node;
                return BaseExpressionRef();
            });
        });
    }
};

class First : public Builtin {
public:
    static constexpr const char *name = "First";

public:
    using Builtin::Builtin;

    void build() {
        message("nofirst", "There is no first element in `1`.");

        builtin(&First::apply);
    }

    inline BaseExpressionRef apply(BaseExpressionPtr x, const Evaluation &evaluation) {
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

	inline BaseExpressionRef apply(BaseExpressionPtr x, const Evaluation &evaluation) {
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

	inline BaseExpressionRef apply(BaseExpressionPtr x, const Evaluation &evaluation) {
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

	inline BaseExpressionRef apply(BaseExpressionPtr x, const Evaluation &evaluation) {
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
        BaseExpressionPtr list,
        BaseExpressionPtr cond,
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
    BaseExpressionPtr imin,
    BaseExpressionPtr imax,
    BaseExpressionPtr di,
    const Evaluation &evaluation,
    ExpressionRef &result) {

    const machine_integer_t imin_integer =
        static_cast<const class MachineInteger *>(imin)->value;
    const machine_integer_t imax_integer =
        static_cast<const class MachineInteger *>(imax)->value;
    const machine_integer_t di_integer =
        static_cast<const class MachineInteger *>(di)->value;

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

	    const F &func = m_func;

	    return expression(evaluation.List, [imin, imax, di, func] (auto &storage) {
		    machine_integer_t index = imin;
		    while (index <= imax) {
			    storage << func(Heap::MachineInteger(index));
			    index += di;
		    }
	    }, n);
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
        BaseExpressionPtr imin_expr,
        BaseExpressionPtr imax_expr,
        BaseExpressionPtr di_expr,
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
        BaseExpressionPtr imin,
        BaseExpressionPtr imax,
        BaseExpressionPtr di,
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
        BaseExpressionPtr imin,
        BaseExpressionPtr imax,
        BaseExpressionPtr di,
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

class IterationFunction {
protected:
    const SymbolRef m_symbol;

    template<typename F>
    ExpressionRef iterate_generic(
        const F &f,
        BaseExpressionPtr imin,
        BaseExpressionPtr imax,
        BaseExpressionPtr di,
        const Evaluation &evaluation) const {

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

            BaseExpressionRef index_copy(index);
            BaseExpressionRef leaf = f(std::move(index_copy));

            type_mask |= leaf->base_type_mask();
            result.emplace_back(std::move(leaf));

            index = expression(Plus, index, di)->evaluate_or_copy(evaluation);
        }

        return expression(evaluation.List, std::move(result), type_mask);
    }

    template<typename F>
    inline ExpressionRef iterate(
        const F &f,
        BaseExpressionPtr imin,
        BaseExpressionPtr imax,
        BaseExpressionPtr di,
        const Evaluation &evaluation) const {

        if (imin->type() == MachineIntegerType &&
            imax->type() == MachineIntegerType &&
            di->type() == MachineIntegerType) {

            ExpressionRef result;
            if (iterate_integer_range(m_symbol,
                machine_integer_table<F>(f),
                imin, imax, di, evaluation, result)) {

                return result;
            }
        }

        return iterate_generic(f, imin, imax, di, evaluation);
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
        BaseExpressionPtr iter,
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
                    const BaseExpressionRef imax(slice[0]->evaluate_or_copy(evaluation));

				    return self->iterate(
					    [&f] (const BaseExpressionRef &) {
                            return f();
                        },
                        evaluation.zero.get(),
                        imax.get(),
					    evaluation.one.get(),
                        evaluation);
			    }

			    case 2: { // {i_Symbol, imax_} or {i_Symbol, {items___}}
				    const auto iterator_expr = slice[0];

				    if (iterator_expr->type() == SymbolType) {
					    Symbol * const iterator = iterator_expr->as_symbol();

					    const BaseExpressionRef &domain = slice[1];
					    const Expression *domain_list = if_list(domain);

					    if (domain_list) {
						    domain_list = if_list(domain_list->evaluate_or_copy(evaluation));
						    if (domain_list) {
							    return domain_list->map(evaluation.List,
									[iterator, &f] (const auto &slice, auto &storage) {
									    const size_t n = slice.size();

									    for (size_t i = 0; i < n; i++) {
										    BaseExpressionRef value = slice[i];
										    storage << scope(
											    iterator,
											    std::move(value),
											    f);
									    }
							        });
						    }
					    } else {
                            const BaseExpressionRef imax(slice[1]->evaluate_or_copy(evaluation));

						    return self->iterate(
                                scoped(iterator->as_symbol(), f),
								evaluation.one.get(),
                                imax.get(),
								evaluation.one.get(),
							    evaluation);
					    }
				    }
				    break;
			    }
			    case 3: { // {i_Symbol, imin_, imax_}
				    const auto iterator = slice[0];

                    const BaseExpressionRef imin(slice[1]->evaluate_or_copy(evaluation));
                    const BaseExpressionRef imax(slice[2]->evaluate_or_copy(evaluation));

				    if (iterator->type() == SymbolType) {
					    return self->iterate(
                            scoped(iterator->as_symbol(), f),
						    imin.get(),
						    imax.get(),
						    evaluation.one.get(),
						    evaluation);
				    }
				    break;
			    }
			    case 4: { // {i_Symbol, imin_, imax_, di_}
				    const auto iterator = slice[0];

				    if (iterator->type() == SymbolType) {
                        const BaseExpressionRef imin(slice[1]->evaluate_or_copy(evaluation));
                        const BaseExpressionRef imax(slice[2]->evaluate_or_copy(evaluation));
                        const BaseExpressionRef di(slice[3]->evaluate_or_copy(evaluation));

					    return self->iterate(
                            scoped(iterator->as_symbol(), f),
						    imin.get(),
                            imax.get(),
                            di.get(),
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
        BaseExpressionPtr expr,
        const BaseExpressionRef *iter_begin,
        const BaseExpressionRef *iter_end,
        const Evaluation &evaluation) const {

        if (iter_end - iter_begin == 1) {
            return m_function.apply([&expr, &evaluation] () {
                return expr->evaluate_or_copy(evaluation);
            }, iter_begin[0].get(), evaluation);
        } else {
            const auto self = this;

            return m_function.apply([self, &expr, iter_begin, iter_end, &evaluation] () {
                return self->apply(expr, iter_begin + 1, iter_end, evaluation);
            }, iter_begin[0].get(), evaluation);
        }
    }

public:
    IterationFunctionRule(const SymbolRef &head, const Definitions &definitions) :
        AtLeastNRule<2>(head, definitions), m_head(head), m_function(head) {
    }

    virtual BaseExpressionRef try_apply(const Expression *expr, const Evaluation &evaluation) const {
        const auto self = this;

        auto result = expr->with_leaves_array([self, &evaluation] (const BaseExpressionRef *leaves, size_t n) {
            return self->apply(leaves[0].get(), leaves + 1, leaves + n, evaluation);
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

    add<Level>();

    add("ListQ",
        Attributes::None, {
            builtin<1>([](BaseExpressionPtr x, const Evaluation &evaluation) {
                if (x->type() == ExpressionType) {
                    return evaluation.Boolean(x->as_expression()->_head == evaluation.List);
                } else {
                    return evaluation.False;
                }
            })
        });

    add("NotListQ",
        Attributes::None, {
            builtin<1>([](BaseExpressionPtr x, const Evaluation &evaluation) {
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
                 [](BaseExpressionPtr x, const Evaluation &evaluation) {
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
				[](BaseExpressionPtr f, BaseExpressionPtr x, const Evaluation &evaluation) {
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
				[](BaseExpressionPtr f, BaseExpressionPtr expr, const Evaluation &evaluation) {
				    if (expr->type() != ExpressionType) {
					    return ExpressionRef();
				    }
					const Expression *list = expr->as_expression();

					return list->map(list->head(), [&f] (const auto &slice, auto &storage) {
						const size_t size = slice.size();

						for (size_t i = 0; i < size; i++) {
							const auto leaf = slice[i];
							storage << expression(f, StaticSlice<1>(&leaf, leaf->base_type_mask()));
						}
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
