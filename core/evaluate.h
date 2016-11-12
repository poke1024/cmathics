#ifndef CMATHICS_EVALUATE_H
#define CMATHICS_EVALUATE_H

template<typename Slice>
using Evaluator = std::function<BaseExpressionRef(
	const BaseExpressionRef &head, const Slice &slice)>;

class heap_storage {
private:
	std::vector<BaseExpressionRef> _leaves;

public:
	inline heap_storage(size_t size) {
		_leaves.reserve(size);
	}

	inline heap_storage &operator<<(const BaseExpressionRef &expr) {
		_leaves.push_back(expr);
		return *this;
	}

	inline heap_storage &operator<<(BaseExpressionRef &&expr) {
		_leaves.push_back(expr);
		return *this;
	}

	inline ExpressionRef to_expression(const BaseExpressionRef &head) {
		return expression(head, std::move(_leaves));
	}
};

template<size_t N>
class direct_storage {
protected:
	InPlaceExpressionRef<N> _expr;
	BaseExpressionRef *_addr;
	BaseExpressionRef *_end;

public:
	inline direct_storage(const BaseExpressionRef &head) :
		_expr(Heap::Expression(head, InPlaceRefsSlice<N>())),
		_addr(_expr->_leaves.late_init()), _end(_addr + N) {
	}

	inline direct_storage &operator<<(const BaseExpressionRef &expr) {
		assert(_addr < _end);
		*_addr++ = expr;
		return *this;
	}

	inline direct_storage &operator<<(BaseExpressionRef &&expr) {
		assert(_addr < _end);
		*_addr++ = expr;
		return *this;
	}

	inline ExpressionRef to_expression() {
		return _expr;
	}
};

template<size_t N, typename F>
inline ExpressionRef tiny_expression(const BaseExpressionRef &head, const F &generate) {
	direct_storage<N> storage(head);
	generate(storage);
	return storage.to_expression();
}

template<typename F>
inline ExpressionRef expression(const BaseExpressionRef &head, const F &generate, size_t size) {
	heap_storage storage(size);
	generate(storage);
	return storage.to_expression(head);
}

template<typename Slice, typename F>
ExpressionRef apply(
	const BaseExpressionRef &head,
	const Slice &slice,
	size_t begin,
	size_t end,
	const F &f,
	bool apply_head,
	TypeMask type_mask) {

	if ((type_mask & slice.type_mask()) != 0) {
		/*if (_extent.use_count() == 1) {
			// FIXME. optimize this case. we do not need to copy here.
		}*/

		for (size_t i0 = begin; i0 < end; i0++) {
			const auto &leaf = slice[i0];

			if ((leaf->type_mask() & type_mask) == 0) {
				continue;
			}

			const auto leaf0 = f(leaf);

			if (leaf0) { // copy is needed now
				const size_t size = slice.size();

				auto generate_leaves = [i0, end, size, type_mask, &slice, &f, &leaf0] (auto storage) {
					for (size_t j = 0; j < i0; j++) {
						storage << slice[j];
					}

					storage << leaf0; // leaves[i0]

					for (size_t j = i0 + 1; j < end; j++) {
						const auto &old_leaf = slice[j];

						if ((old_leaf->type_mask() & type_mask) == 0) {
							storage << old_leaf;
						} else {
							auto new_leaf = f(old_leaf);
							if (new_leaf) {
								storage << new_leaf;
							} else {
								storage << old_leaf;
							}
						}
					}

					for (size_t j = end; j < size; j++) {
						storage << slice[j];
					}

				};

				switch (size) {
					case 1:
						return tiny_expression<1>(head, generate_leaves);
					case 2:
						return tiny_expression<2>(head, generate_leaves);
					case 3:
						return tiny_expression<3>(head, generate_leaves);
					default:
						return expression(head, generate_leaves, size);
				}
			}
		}
	}

	if (apply_head) {
		return expression(head, slice);
	} else {
		return RefsExpressionRef();
	}
}

template<typename Slice>
BaseExpressionRef evaluate(
	const ExpressionRef &self,
	const BaseExpressionRef &head,
	const Slice &slice,
	const Evaluation &evaluation) {

	const auto head_symbol = static_cast<const Symbol *>(head.get());
	const auto attributes = head_symbol->attributes;

	// only one type of Hold attribute can be set at a time
	assert(count(
			attributes,
			Attributes::HoldFirst +
			Attributes::HoldRest +
			Attributes::HoldAll +
			Attributes::HoldAllComplete) <= 1);

	size_t eval_leaf_start, eval_leaf_stop;

	if (attributes & Attributes::HoldFirst) {
		eval_leaf_start = 1;
		eval_leaf_stop = slice.size();
	} else if (attributes & Attributes::HoldRest) {
		eval_leaf_start = 0;
		eval_leaf_stop = 1;
	} else if (attributes & Attributes::HoldAll) {
		// TODO flatten sequences
		eval_leaf_start = 0;
		eval_leaf_stop = 0;
	} else if (attributes & Attributes::HoldAllComplete) {
		// no more evaluation is applied
		return RefsExpressionRef();
	} else {
		eval_leaf_start = 0;
		eval_leaf_stop = slice.size();
	}

	assert(0 <= eval_leaf_start);
	assert(eval_leaf_start <= eval_leaf_stop);
	assert(eval_leaf_stop <= slice.size());

	ExpressionRef intermediate_form = apply(
		head,
		slice,
		eval_leaf_start,
		eval_leaf_stop,
		[&evaluation](const BaseExpressionRef &leaf) {
			return leaf->evaluate(leaf, evaluation);
		},
		head != self->_head,
		MakeTypeMask(ExpressionType) | MakeTypeMask(SymbolType));

	if (!intermediate_form) {
		intermediate_form = boost::static_pointer_cast<const Expression>(self);
	}
	// Step 3
	// Apply UpValues for leaves
	// TODO

	// Step 4
	// Evaluate the head with leaves. (DownValue)

	for (const Rule &rule : head_symbol->down_rules) {
		auto result = rule(intermediate_form, evaluation);
		if (result) {
			return result;
		}
	}

	return BaseExpressionRef();
}

/*template<
	bool HoldFirst>
void fill_evaluators(Evaluator[n_slice_types] &evaluators) {
	Evaluator[] evaluators = {
		evaluate<RefsSlice, HoldFirst>,
		evaluate<InPlaceRefsSlice<0>, HoldFirst>,
		evaluate<InPlaceRefsSlice<1>, HoldFirst>,
		evaluate<InPlaceRefsSlice<2>, HoldFirst>,
		evaluate<InPlaceRefsSlice<3>, HoldFirst>,
	}
}*/

#endif //CMATHICS_EVALUATE_H
