// @formatter:off

#include "lists.h"
#include "../core/definitions.h"

inline const Expression *if_list(const BaseExpressionPtr item) {
    if (item->type() == ExpressionType) {
        const Expression *expr =item->as_expression();
        if (expr->head()->extended_type() == SymbolList) {
            return expr;
        }
    }
    return nullptr;
}

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
                    throw InvalidError();
            }
        } else if (spec->extended_type() == SymbolAll) {
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

        if (node->type() == ExpressionType) {
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

		        return conditional_map_indexed_all(
			        new_head ? new_head : head, new_head,
			        lambda(recurse),
			        slice, 0, slice.size(),
			        evaluation);
	        };

            modified_node = expr->with_slice<Optimize>(apply);
        }

        if (is_in_level(current, depth)) {
            return std::make_tuple(callback(modified_node ? modified_node : node, *pos), depth);
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
        if (x->type() != ExpressionType) {
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
        if (x->type() != ExpressionType) {
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

        if (list->type() != ExpressionType) {
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
};

class Cases : public Builtin {
private:
    CachedBaseExpressionRef m_default_ls;

public:
    static constexpr const char *name = "Cases";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        m_default_ls.initialize(expression(
            runtime.definitions().symbols().List, Pool::MachineInteger(1)));

	    const OptionsInitializerList options = {
            {"Heads", offsetof(CasesOptions, Heads), "False"}
        };

        builtin(options, &Cases::apply2);
        builtin(options, &Cases::apply3);
    }

    inline BaseExpressionRef apply2(
        BaseExpressionPtr list,
        BaseExpressionPtr patt,
        const CasesOptions &options,
        const Evaluation &evaluation) {

	    return apply3(list, patt, m_default_ls.get(), options, evaluation);
    }

    inline BaseExpressionRef apply3(
        BaseExpressionPtr list,
        BaseExpressionPtr patt,
        BaseExpressionPtr ls,
        const CasesOptions &options,
        const Evaluation &evaluation) {

        if (list->type() != ExpressionType) {
            return expression(evaluation.List);
        }

        try {
            const Levelspec levelspec(ls);

	        const RuleForm rule_form(patt);

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

	        if (rule_form.is_rule()) {
                return match(
                    std::make_tuple(rule_form.left_side(), rule_form.right_side()),
                    generate,
		            patt->as_expression(),
                    evaluation);
            } else {
                return match(
                    std::make_tuple(patt),
                    generate,
	                nullptr,
                    evaluation);
	        }
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
                    store(Pool::MachineInteger(x));
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
			    store(func(Pool::MachineInteger(index)));
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
		rule("Range[imax_]", "Range[1, imax, 1]");
		rule("Range[imin_, imax_]", "Range[imin, imax, 1]");
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
						store(Pool::MachineReal(x));
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
			const ExtendedType if_continue =
				expression(LessEqual, index, imax)->
					evaluate_or_copy(evaluation)->extended_type();

			if (if_continue == SymbolFalse) {
				break;
			} else if (if_continue != SymbolTrue) {
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
			imin->base_type_mask() |
			imax->base_type_mask() |
			di->base_type_mask();

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

        if (imin->type() == MachineIntegerType &&
            imax->type() == MachineIntegerType &&
            di->type() == MachineIntegerType) {

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
					    const Expression *domain_list = if_list(domain.get());

					    if (domain_list) {
						    const BaseExpressionRef domain2 =
								domain_list->evaluate_or_copy(evaluation);
						    domain_list = if_list(domain2.get());
						    if (domain_list) {
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
        return new IterationFunctionRule<F>(head, definitions);
    };
}

void Builtins::Lists::initialize() {

    add<Level>();

    add("ListQ",
        Attributes::None, {
            builtin<1>([](ExpressionPtr, BaseExpressionPtr x, const Evaluation &evaluation) {
                if (x->type() == ExpressionType) {
                    return evaluation.Boolean(x->as_expression()->_head.get() == evaluation.List);
                } else {
                    return evaluation.False;
                }
            })
        });

    add("NotListQ",
        Attributes::None, {
            builtin<1>([](ExpressionPtr, BaseExpressionPtr x, const Evaluation &evaluation) {
                if (x->type() == ExpressionType) {
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
				[](ExpressionPtr, BaseExpressionPtr f, BaseExpressionPtr x, const Evaluation &evaluation) {
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
    add<Cases>();

	add("Map",
	    Attributes::None, {
			builtin<2>(
				[](ExpressionPtr, BaseExpressionPtr func, BaseExpressionPtr expr, const Evaluation &evaluation) {
				    if (expr->type() != ExpressionType) {
					    return ExpressionRef();
				    }
					const Expression *list = expr->as_expression();

					return list->parallel_map(list->head(), [&func] (const auto &leaf) {
						return expression(func, StaticSlice<1>(&leaf, leaf->base_type_mask()));
					});
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
