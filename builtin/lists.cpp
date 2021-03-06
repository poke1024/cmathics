// @formatter:off

#include "lists.h"
#include "levelspec.tcc"
#include "../core/definitions.h"

class ListBoxes {
private:
    CachedBaseExpressionRef m_comma;
    CachedBaseExpressionRef m_comma_space;

public:
    ListBoxes() {
        m_comma.initialize(String::construct(","));
        m_comma_space.initialize(String::construct(", "));
    }

    inline BaseExpressionRef operator()(
        const BaseExpressionPtr items,
        const BaseExpressionPtr form,
        const Evaluation &evaluation) const {

        assert(items->is_expression()); // must be a Sequence
        const size_t n = items->as_expression()->size();

        if (n > 1) {
            BaseExpressionPtr sep;

            switch (form->symbol()) {
	            case S::OutputForm:
	            case S::InputForm:
                    sep = m_comma_space.get();
                    break;
                default:
                    sep = m_comma.get();
                    break;
            }

           const BaseExpressionRef list = items->as_expression()->with_slice(
                [n, form, sep, &evaluation] (const auto &slice) {
                return expression(evaluation.List, sequential(
                    [&slice, form, sep, &evaluation] (auto &store) {
                        const size_t n = slice.size();
                        for (size_t i = 0; i < n; i++) {
                            if (i > 0) {
                                store(BaseExpressionRef(sep));
                            }
                            store(expression(evaluation.MakeBoxes, slice[i], form));
                        }

                    }, n + (n - 1)));
                });

            return expression(evaluation.RowBox, list);
        } else if (n == 1) {
            const auto * const leaves = items->as_expression()->n_leaves<1>();
            return expression(evaluation.MakeBoxes, leaves[0], form);
        } else {
            return BaseExpressionRef();
        }
    }
};

class List : public Builtin {
private:
    CachedBaseExpressionRef m_open;
    CachedBaseExpressionRef m_close;
    ListBoxes m_boxes;

public:
    static constexpr const char *name = "List";

public:
    using Builtin::Builtin;

    inline BaseExpressionRef apply_makeboxes(
        const BaseExpressionPtr items,
        const BaseExpressionPtr form,
        const Evaluation &evaluation) {

        return expression(evaluation.RowBox, expression(evaluation.List, sequential(
            [this, items, form, &evaluation] (auto &store) {
                store(BaseExpressionRef(m_open));
                BaseExpressionRef item = m_boxes(items, form, evaluation);
                if (item) {
                    store(std::move(item));
                }
                store(BaseExpressionRef(m_close));
            })));
    }

    void build(Runtime &runtime) {
        m_open.initialize(String::construct("{"));
        m_close.initialize(String::construct("}"));
	    builtin(
            "MakeBoxes[{items___}, f:StandardForm|TraditionalForm|OutputForm|InputForm]",
            &List::apply_makeboxes);
    }
};

struct LevelOptions {
	BaseExpressionPtr Heads;
};

class Level : public Builtin {
public:
    static constexpr const char *name = "Level";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
	    const OptionsInitializerList options = {
		    {"Heads", offsetof(LevelOptions, Heads), "False"}
	    };

	    builtin(options, &Level::apply);
    }

    inline BaseExpressionRef apply(
	    BaseExpressionPtr expr,
	    BaseExpressionPtr ls,
	    const LevelOptions &options,
	    const Evaluation &evaluation) {

	    try {
		    const Levelspec levelspec(ls);

		    return expression(evaluation.List, sequential(
				[&options, &expr, &levelspec, &evaluation] (auto &store) {
				    levelspec.walk_immutable(
					    BaseExpressionRef(expr), options.Heads->is_true(),
					    [&store] (const auto &node) {
						    store(BaseExpressionRef(node));
					    },
					    evaluation);
				}));
	    } catch (const Levelspec::InvalidError&) {
		    evaluation.message(m_symbol, "level", ls);
		    return BaseExpressionRef();
	    }
    }
};

class First : public Builtin {
public:
    static constexpr const char *name = "First";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'First[$expr$]'
        <dd>returns the first element in $expr$.
    </dl>

    'First[$expr$]' is equivalent to '$expr$[[1]]'.

    >> First[{a, b, c}]
     = a
    >> First[a + b + c]
     = a
    >> First[x]
     : Nonatomic expression expected.
     = First[x]
	)";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        message("nofirst", "There is no first element in `1`.");

        builtin(&First::apply);
    }

    inline BaseExpressionRef apply(BaseExpressionPtr x, const Evaluation &evaluation) {
        if (!x->is_expression()) {
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

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Last[$expr$]'
        <dd>returns the last element in $expr$.
    </dl>

    'Last[$expr$]' is equivalent to '$expr$[[-1]]'.

    >> Last[{a, b, c}]
     = c
    >> Last[x]
     : Nonatomic expression expected.
     = Last[x]
	)";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        message("nolast", "There is no last element in `1`.");

        builtin(&Last::apply);
    }

	inline BaseExpressionRef apply(BaseExpressionPtr x, const Evaluation &evaluation) {
        if (!x->is_expression()) {
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

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Most[$expr$]'
        <dd>returns $expr$ with the last element removed.
    </dl>

    'Most[$expr$]' is equivalent to '$expr$[[;;-2]]'.

    >> Most[{a, b, c}]
     = {a, b}
    >> Most[a + b + c]
     = a + b
    >> Most[x]
     : Nonatomic expression expected.
     = Most[x]

    #> A[x__] := 7 /; Length[{x}] == 3;
    #> Most[A[1, 2, 3, 4]]
     = 7
    #> ClearAll[A];
	)";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        message("nomost", "Most is not applicable to `1`.");

        builtin(&Most::apply);
    }

	inline BaseExpressionRef apply(BaseExpressionPtr x, const Evaluation &evaluation) {
        if (!x->is_expression()) {
            evaluation.message(m_symbol, "normal");
        } else {
            const Expression *expr = x->as_expression();
            if (expr->size() < 1) {
                evaluation.message(m_symbol, "nomost", expr);
            } else {
                return expr->slice(expr->head(), 0, -1);
            }
        }
        return BaseExpressionRef();
    }
};

class Rest : public Builtin {
public:
    static constexpr const char *name = "Rest";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Rest[$expr$]'
        <dd>returns $expr$ with the first element removed.
    </dl>

    'Rest[$expr$]' is equivalent to '$expr$[[2;;]]'.

    >> Rest[{a, b, c}]
     = {b, c}
    >> Rest[a + b + c]
     = b + c
    >> Rest[x]
     : Nonatomic expression expected.
     = Rest[x]
	)";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        message("norest", "Rest is not applicable to `1`.");

        builtin(&Rest::apply);
    }

	inline BaseExpressionRef apply(BaseExpressionPtr x, const Evaluation &evaluation) {
        if (!x->is_expression()) {
            evaluation.message(m_symbol, "normal");
        } else {
            const Expression *expr = x->as_expression();
            if (expr->size() < 1) {
                evaluation.message(m_symbol, "norest", expr);
            } else {
                return expr->slice(expr->head(), 1);
            }
        }
        return BaseExpressionRef();
    }
};

class Select : public Builtin {
public:
    static constexpr const char *name = "Select";

    static constexpr const char *docs = R"(
    """
    <dl>
    <dt>'Select[{$e1$, $e2$, ...}, $f$]'
        <dd>returns a list of the elements $ei$ for which $f$[$ei$]
        returns 'True'.
    </dl>

    Find numbers greater than zero:
    >> Select[{-3, 0, 1, 3, a}, #>0&]
     = {1, 3}

    'Select' works on an expression with any head:
    >> Select[f[a, 2, 3], NumberQ]
     = f[2, 3]

    >> Select[a, True]
     : Nonatomic expression expected.
     = Select[a, True]

    #> A[x__] := 31415 /; Length[{x}] == 3;
    #> Select[A[5, 2, 7, 1], OddQ]
     = 31415
    #> ClearAll[A];
    """
	)";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&Select::apply);
    }

	inline BaseExpressionRef apply(
        BaseExpressionPtr list,
        BaseExpressionPtr cond,
        const Evaluation &evaluation) {

        if (!list->is_expression()) {
            evaluation.message(m_symbol, "normal");
        } else {
	        return list->as_expression()->with_slice(
			    [list, &cond, &evaluation] (const auto &slice) {

		            const size_t size = slice.size();

					LeafVector remaining;
			        remaining.reserve(size);

			        for (size_t i = 0; i < size; i++) {
				        const auto leaf = slice[i];
				        if (expression(cond, leaf)->evaluate_or_copy(evaluation)->is_true()) {
					        remaining.push_back_copy(leaf);
				        }
			        }

			        return expression(list->as_expression()->head(), std::move(remaining));
	           }
	        );
        }
        return BaseExpressionRef();
    }
};

struct CasesOptions {
    BaseExpressionPtr Heads;

	static OptionsInitializerList meta() {
		return {
			{"Heads", offsetof(CasesOptions, Heads), "False"}
		};
	};
};


class Cases : public Builtin {
public:
    static constexpr const char *name = "Cases";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
	    builtin(
		    "Cases[list_, patt_, Shortest[ls_:{1}], OptionsPattern[Cases]]",
		    &Cases::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr list,
        BaseExpressionPtr patt,
        BaseExpressionPtr ls,
        const CasesOptions &options,
        const Evaluation &evaluation) {

        if (!list->is_expression()) {
            return expression(evaluation.List);
        }

        try {
            const Levelspec levelspec(ls);

	        const auto generate = [&list, &evaluation, &options, &levelspec] (const auto &match) {
		        return expression(
			        evaluation.List, sequential([&list, &evaluation, &options, &levelspec, &match] (auto &store) {
				        levelspec.walk_immutable(
						    BaseExpressionRef(list),
						    options.Heads->is_true(),
				            [&store, &match] (const BaseExpressionRef &node) {
                                BaseExpressionRef result = match(node);
                                if (result) {
                                    store(std::move(result));
                                }
				            },
					        evaluation);
			        }));
	        };

            return match(patt, generate, evaluation);

        } catch (const Levelspec::InvalidError&) {
            evaluation.message(m_symbol, "level", ls);
            return BaseExpressionRef();
        }
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
    UnsafeExpressionRef &result) {

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
            sequential([imin, imax, di] (auto &store) {
                for (machine_integer_t x = imin; x <= imax; x += di) {
                    store(MachineInteger::construct(x));
                }
            }, n));
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

	    return expression(evaluation.List, sequential([imin, imax, di, func] (auto &store) {
		    machine_integer_t index = imin;
		    while (index <= imax) {
			    store(func(MachineInteger::construct(index)));
			    index += di;
		    }
	    }, n));
    }
};

class Range : public Builtin {
public:
	static constexpr const char *name = "Range";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("Range[imax_]", "Range[1, imax, 1]");
		builtin("Range[imin_, imax_]", "Range[imin, imax, 1]");
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
				sequential([&leaves] (auto &store) {
					for (machine_real_t x : leaves) {
						store(MachineReal::construct(x));
					}
				},
				leaves.size())
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

		LeafVector result;

		UnsafeBaseExpressionRef index = imin;
		while (true) {
			const auto if_continue =
				expression(LessEqual, index, imax)->
					evaluate_or_copy(evaluation)->symbol();

			if (if_continue == S::False) {
				break;
			} else if (if_continue != S::True) {
				evaluation.message(m_symbol, "iterb");
				return BaseExpressionRef();
			}

			result.push_back(BaseExpressionRef(index));

			index = expression(Plus, index, di)->evaluate_or_copy(evaluation);
		}

		return expression(evaluation.List, std::move(result));
	}

public:
	BaseExpressionRef apply(
        BaseExpressionPtr imin,
        BaseExpressionPtr imax,
        BaseExpressionPtr di,
		const Evaluation &evaluation) {

		const TypeMask type_mask =
			imin->type_mask() |
			imax->type_mask() |
			di->type_mask();

		if (type_mask & make_type_mask(MachineRealType)) { // expression contains a MachineReal
			return MachineReal(imin, imax, di, evaluation);
		}

		constexpr TypeMask machine_int_mask = make_type_mask(MachineIntegerType);

		if ((type_mask & machine_int_mask) == type_mask) { // expression is all MachineIntegers
			UnsafeExpressionRef result;
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

        LeafVector result;

        UnsafeBaseExpressionRef index = imin;
        while (true) {
            const auto if_continue =
                expression(LessEqual, index, imax)->
                    evaluate_or_copy(evaluation)->symbol();

            if (if_continue == S::False) {
                break;
            } else if (if_continue != S::True) {
                evaluation.message(m_symbol, "iterb");
                return ExpressionRef();
            }

            BaseExpressionRef index_copy(index);
            BaseExpressionRef leaf = f(std::move(index_copy));

            result.push_back(std::move(leaf));

            index = expression(Plus, index, di)->evaluate_or_copy(evaluation);
        }

        return expression(evaluation.List, std::move(result));
    }

    template<typename F>
    inline ExpressionRef iterate(
        const F &f,
        BaseExpressionPtr imin,
        BaseExpressionPtr imax,
        BaseExpressionPtr di,
        const Evaluation &evaluation) const {

        if (imin->is_machine_integer() &&
            imax->is_machine_integer() &&
            di->is_machine_integer()) {

            UnsafeExpressionRef result;
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

        if (!iter->is_list()) {
            evaluation.message(m_symbol, "iterb");
            return ExpressionRef();
        }

        const ExpressionPtr list = iter->as_expression();
	    const Table * const self = this;
	    return list->with_slice_c([self, &f, &evaluation] (const auto &slice) {
		    switch (slice.size()) {
			    case 1: { // {imax_}
                    const BaseExpressionRef imax(slice[0]->evaluate_or_copy(evaluation));

				    return self->iterate(
					    [&f] (const BaseExpressionRef &) {
                            return f();
                        },
                        evaluation.definitions.zero.get(),
                        imax.get(),
					    evaluation.definitions.one.get(),
                        evaluation);
			    }

			    case 2: { // {i_Symbol, imax_} or {i_Symbol, {items___}}
				    const auto iterator_expr = slice[0];

				    if (iterator_expr->is_symbol()) {
					    Symbol * const iterator = iterator_expr->as_symbol();

					    const BaseExpressionRef &domain = slice[1];

					    if (domain->is_list()) {
    					    ExpressionPtr domain_list =
                                domain->as_expression();
						    const BaseExpressionRef domain2 =
								domain_list->evaluate_or_copy(evaluation);
						    if (domain2->is_list()) {
    						    domain_list = domain2->as_expression();
							    return domain_list->map(evaluation.List,
									[iterator, &f] (const auto &value) {
									    return scope(
										    iterator,
										    BaseExpressionRef(value),
										    f);
							        });
						    }
					    } else {
                            const BaseExpressionRef imax(slice[1]->evaluate_or_copy(evaluation));

						    return self->iterate(
                                scoped(iterator->as_symbol(), f),
								evaluation.definitions.one.get(),
                                imax.get(),
								evaluation.definitions.one.get(),
							    evaluation);
					    }
				    }
				    break;
			    }
			    case 3: { // {i_Symbol, imin_, imax_}
				    const auto iterator = slice[0];

                    const BaseExpressionRef imin(slice[1]->evaluate_or_copy(evaluation));
                    const BaseExpressionRef imax(slice[2]->evaluate_or_copy(evaluation));

				    if (iterator->is_symbol()) {
					    return self->iterate(
                            scoped(iterator->as_symbol(), f),
						    imin.get(),
						    imax.get(),
						    evaluation.definitions.one.get(),
						    evaluation);
				    }
				    break;
			    }
			    case 4: { // {i_Symbol, imin_, imax_, di_}
				    const auto iterator = slice[0];

				    if (iterator->is_symbol()) {
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
class IterationFunctionRule :
    public AtLeastNRule<2>,
    public ExtendedHeapObject<IterationFunctionRule<F>> {

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
    IterationFunctionRule(const SymbolRef &head, const Evaluation &evaluation) :
        AtLeastNRule<2>(head, evaluation), m_head(head), m_function(head) {
    }

    virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

        const BaseExpressionRef result = expr->with_leaves_array(
            [this, &evaluation] (const BaseExpressionRef *leaves, size_t n) {
                return apply(leaves[0].get(), leaves + 1, leaves + n, evaluation);
            });

		return result;
    }
};

template<typename F>
NewRuleRef make_iteration_function_rule() {
    return [] (const SymbolRef &head, const Evaluation &evaluation) {
        return IterationFunctionRule<F>::construct(head, evaluation);
    };
}

void Builtins::Lists::initialize() {
    add<List>();

    add<Level>();

    add("ListQ",
        Attributes::None, {
            builtin<1>([](ExpressionPtr, BaseExpressionPtr x, const Evaluation &evaluation) {
                if (x->is_expression()) {
                    return evaluation.Boolean(x->as_expression()->_head.get() == evaluation.List);
                } else {
                    return evaluation.False;
                }
            })
        });

    add("NotListQ",
        Attributes::None, {
            builtin<1>([](ExpressionPtr, BaseExpressionPtr x, const Evaluation &evaluation) {
                if (x->is_expression()) {
                    return evaluation.Boolean(x->as_expression()->_head.get() != evaluation.List);
                } else {
                    return evaluation.True;
                }
            })
        });

    add("Length",
        Attributes::None, {
             builtin<1>(
                 [](ExpressionPtr, BaseExpressionPtr x, const Evaluation &evaluation) {
                     if (!x->is_expression()) {
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
				[](ExpressionPtr, BaseExpressionPtr f, BaseExpressionPtr x, const Evaluation &evaluation) -> BaseExpressionRef {
				    if (!x->is_expression()) {
					    return BaseExpressionRef();
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
    add<Cases>();

	add("Map",
	    Attributes::None, {
			builtin<2>(
				[](ExpressionPtr, BaseExpressionPtr func, BaseExpressionPtr expr, const Evaluation &evaluation) {
				    if (!expr->is_expression()) {
					    return ExpressionRef();
				    }
					const Expression *list = expr->as_expression();

					return list->parallel_map(list->head(), [func] (const auto &leaf) {
						return expression(func, TinySlice<1>(&leaf, leaf->type_mask()));
					}, evaluation);
			    }
		    )
	    });

	add<Range>();

	add("Mean",
	    Attributes::None, {
			down("Mean[x_List]", "Total[x] / Length[x]"),
	    }, R"(
            >> Mean[{26, 64, 36}]
             = 42

            >> Mean[{1, 1, 2, 3, 5, 8}]
             = 10 / 3

            >> Mean[{a, b}]
             = (a + b) / 2
        )");

	add("Total",
	    Attributes::None, {
            down("Total[head_]", "Apply[Plus, head]"),
            down("Total[head_, n_]", "Apply[Plus, Flatten[head, n]]"),
	    });

    add("Table",
        Attributes::HoldAll, {
            make_iteration_function_rule<Table>()
        });
}
