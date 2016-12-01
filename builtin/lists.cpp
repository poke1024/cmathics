#include "lists.h"
#include "../core/definitions.h"

template<typename F>
BaseExpressionRef compute(TypeMask mask, const F &f) {
	// expression contains a Real
	if (mask & MakeTypeMask(MachineRealType)) {
		return f.template compute<double>();
	}

	constexpr TypeMask machine_int_mask = MakeTypeMask(MachineIntegerType);
	// expression is all MachineIntegers
	if ((mask & machine_int_mask) == mask) {
		return f.template compute<int64_t>();
	}

	constexpr TypeMask int_mask = MakeTypeMask(BigIntegerType) | MakeTypeMask(MachineIntegerType);
	// expression is all Integers
	if ((mask & int_mask) == mask) {
		return f.template compute<mpz_class>();
	}

	constexpr TypeMask rational_mask = MakeTypeMask(BigRationalType);
	// expression is all Rationals
	if ((mask & rational_mask) == mask) {
		return f.template compute<mpq_class>();
	}

	return BaseExpressionRef(); // cannot evaluate
}

class RangeComputation {
private:
	const BaseExpressionRef &_imin;
	const BaseExpressionRef &_imax;
	const BaseExpressionRef &_di;
	const Evaluation &_evaluation;

public:
	inline RangeComputation(
		const BaseExpressionRef &imin,
		const BaseExpressionRef &imax,
		const BaseExpressionRef &di,
		const Evaluation &evaluation) : _imin(imin), _imax(imax), _di(di), _evaluation(evaluation) {
	}

	template<typename T>
	BaseExpressionRef compute() const {
		const T imin = to_primitive<T>(_imin);
		const T imax = to_primitive<T>(_imax);
		const T di = to_primitive<T>(_di);

		std::vector<T> leaves;
		for (T x = imin; x <= imax; x += di) {
			leaves.push_back(x);
		}

		return expression(_evaluation.List, PackedSlice<T>(std::move(leaves)));
	}
};

BaseExpressionRef Range(
	const BaseExpressionRef &imin,
	const BaseExpressionRef &imax,
	const BaseExpressionRef &di,
	const Evaluation &evaluation) {

	return compute(
		imin->base_type_mask() | imax->base_type_mask() | di->base_type_mask(),
		RangeComputation(imin, imax, di, evaluation));
}

inline const Expression *if_list(const BaseExpressionRef &item) {
	if (item->type() == ExpressionType) {
		const Expression *expr = static_cast<const Expression*>(item.get());
		if (expr->head()->extended_type() == SymbolList) {
			return expr;
		}
	}
	return nullptr;
}

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

	BaseExpressionRef index = imin;
	while (true) {
		const ExtendedType if_continue =
				expression(LessEqual, index, imax)->
						evaluate_or_copy(evaluation)->extended_type();

		if (if_continue == SymbolFalse) {
			break;
		} else if (if_continue != SymbolTrue) {
			// FIXME error
			break;
		}

		if (SYMBOL) {
			result.push_back(scope(iterator, index, [&expr, &evaluation] () {
				return expr->evaluate_or_copy(evaluation);
			}));
		} else {
			result.push_back(expr->evaluate_or_copy(evaluation));
		}

		index = expression(Plus, index, di)->evaluate_or_copy(evaluation);
	}

	return expression(evaluation.List, std::move(result));
}

void Builtin::Lists::initialize() {

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

	add("First",
	    Attributes::None,
        [] (RulesBuilder &build, const SymbolRef First) {
            build.builtin<1>(
                [First] (const BaseExpressionRef &x, const Evaluation &evaluation) {
                    if (x->type() != ExpressionType) {
                        evaluation.message(First, "normal");
                        return BaseExpressionRef();
                    }
                    const Expression *expr = static_cast<const Expression*>(x.get());
                    if (expr->size() < 1) {
                        throw std::runtime_error("Expression is empty");
                    }
                    return expr->leaf(0);
                }
            );
		});

	add("Length",
	    Attributes::None, {
			builtin<1>(
			    [](const BaseExpressionRef &x, const Evaluation &evaluation) {
				    if (x->type() != ExpressionType) {
					    return from_primitive(machine_integer_t(0));
				    } else {
					    const Expression *list = static_cast<const Expression*>(x.get());
					    return from_primitive(machine_integer_t(list->size()));
				    }
			    }
	        )
	    });

	add("Map",
	    Attributes::None, {
			builtin<2>(
				[](const BaseExpressionRef &f, const BaseExpressionRef &expr, const Evaluation &evaluation) {
				    if (expr->type() != ExpressionType) {
					    return BaseExpressionRef();
				    }
				    const Expression *list = static_cast<const Expression*>(expr.get());
				    return list->map(f, evaluation);
			    }
		    )
	    });

	add("Most",
	    Attributes::None, {
			builtin<1>(
				[](const BaseExpressionRef &x, const Evaluation &evaluation) -> BaseExpressionRef {
				    if (x->type() != ExpressionType) {
					    return BaseExpressionRef();
				    }
				    const Expression *list = static_cast<const Expression*>(x.get());
				    return list->slice(0, -1);
			    }
		    )
	    });

	add("Range",
	    Attributes::None, {
			rewrite("Range[imax_]", "Range[1, imax, 1]"),
		    rewrite("Range[imin_, imax_]", "Range[imin, imax, 1]"),
		    builtin<3>(
				Range
		    )
	    });


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
			builtin<2>([] (
				const BaseExpressionRef &expr,
				const BaseExpressionRef &iter,
				const Evaluation &evaluation) -> ExpressionRef {

				const Expression *list = if_list(iter);
				if (!list) {
					return ExpressionRef();
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

				return ExpressionRef();
			})
        }
	);
}
