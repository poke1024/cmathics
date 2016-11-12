#ifndef CMATHICS_EVALUATE_H
#define CMATHICS_EVALUATE_H

#include "leaves.h"

typedef std::function<BaseExpressionRef(
	const ExpressionRef &self,
	const BaseExpressionRef &head,
	const void *slice_ptr,
	const Evaluation &evaluation)> Evaluator;

typedef std::pair<size_t, size_t> eval_range;

struct _HoldNone {
	static bool const do_eval = true;

	template<typename Slice>
	static inline eval_range eval(const Slice &slice) {
		return eval_range(0, slice.size());
	}
};

struct _HoldFirst {
	static bool const do_eval = true;

	template<typename Slice>
	static inline eval_range eval(const Slice &slice) {
		return eval_range(1, slice.size());
	}
};

struct _HoldRest {
	static bool const do_eval = true;

	template<typename Slice>
	static inline eval_range eval(const Slice &slice) {
		return eval_range(0, 1);
	}
};

struct _HoldAll {
	static bool const do_eval = true;

	template<typename Slice>
	static inline eval_range eval(const Slice &slice) {
		// TODO flatten sequences
		return eval_range(0, 0);
	}
};

struct _HoldAllComplete {
	static bool const do_eval = false;

	template<typename Slice>
	static inline eval_range eval(const Slice &slice) {
		return eval_range(0, 0);
	}
};

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

template<typename Slice, typename Hold>
BaseExpressionRef evaluate(
	const ExpressionRef &self,
	const BaseExpressionRef &head,
	const void *slice_ptr,
	const Evaluation &evaluation) {

	if (!Hold::do_eval) {
		// no more evaluation is applied
		return RefsExpressionRef();
	}

	const Slice &slice = *static_cast<const Slice*>(slice_ptr);

	const auto head_symbol = static_cast<const Symbol *>(head.get());

	const eval_range eval_leaf(Hold::eval(slice));

	assert(0 <= eval_leaf.first);
	assert(eval_leaf.first <= eval_leaf.second);
	assert(eval_leaf.second <= slice.size());

	ExpressionRef intermediate_form = apply(
		head,
		slice,
		eval_leaf.first,
		eval_leaf.second,
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

class Evaluate {
private:
	Evaluator _vtable[NumberOfSliceTypes];

public:
	template<typename Hold>
	void fill() {
		static_assert(1 + InPlaceSlice3Code - RefsSliceCode == NumberOfSliceTypes, "slice code ids error");
		_vtable[RefsSliceCode] = ::evaluate<RefsSlice, Hold>;
		_vtable[PackSliceMachineIntegerCode] = ::evaluate<PackSlice<machine_integer_t>, Hold>;
		_vtable[PackSliceMachineRealCode] = ::evaluate<PackSlice<machine_real_t>, Hold>;
		_vtable[PackSliceBigIntegerCode] = ::evaluate<PackSlice<mpz_class>, Hold>;
		_vtable[PackSliceRationalCode] = ::evaluate<PackSlice<mpq_class>, Hold>;
		_vtable[PackSliceStringCode] = ::evaluate<PackSlice<std::string>, Hold>;
		_vtable[InPlaceSlice0Code] = ::evaluate<InPlaceRefsSlice<0>, Hold>;
		_vtable[InPlaceSlice1Code] = ::evaluate<InPlaceRefsSlice<1>, Hold>;
		_vtable[InPlaceSlice2Code] = ::evaluate<InPlaceRefsSlice<2>, Hold>;
		_vtable[InPlaceSlice3Code] = ::evaluate<InPlaceRefsSlice<3>, Hold>;
	}

	inline BaseExpressionRef operator()(
		const ExpressionRef &self,
		const BaseExpressionRef &head,
		SliceTypeId slice_id,
		const void *slice_ptr,
		const Evaluation &evaluation) const {

		return _vtable[slice_id](self, head, slice_ptr, evaluation);
	}
};

class EvaluateDispatch {
private:
	static EvaluateDispatch *s_instance;

public:

private:
	Evaluate _hold_none;
	Evaluate _hold_first;
	Evaluate _hold_rest;
	Evaluate _hold_all;
	Evaluate _hold_all_complete;

	EvaluateDispatch();

public:
	static void init();

	static const Evaluate &pick(Attributes attributes);
};

#endif //CMATHICS_EVALUATE_H
