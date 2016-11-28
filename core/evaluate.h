#ifndef CMATHICS_EVALUATE_H
#define CMATHICS_EVALUATE_H

#include "leaves.h"

typedef std::function<BaseExpressionRef(
    const Expression *self,
	const BaseExpressionRef &head,
	const Slice &slice,
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
	TypeMask _type_mask;

public:
	inline heap_storage(size_t size) : _type_mask(0) {
		_leaves.reserve(size);
	}

	inline heap_storage &operator<<(const BaseExpressionRef &expr) {
		_type_mask |= expr->base_type_mask();
		_leaves.push_back(expr);
		return *this;
	}

	inline heap_storage &operator<<(BaseExpressionRef &&expr) {
		_type_mask |= expr->base_type_mask();
		_leaves.push_back(expr);
		return *this;
	}

	inline ExpressionRef to_expression(const BaseExpressionRef &head) {
		return expression(head, std::move(_leaves), _type_mask);
	}
};

class direct_storage {
protected:
	ExpressionRef _expr;
	BaseExpressionRef *_addr;
	BaseExpressionRef *_end;
	TypeMask _type_mask;
	TypeMask *_type_mask_ptr;

public:
	inline direct_storage(const BaseExpressionRef &head, size_t n) : _type_mask(0) {
		std::tie(_expr, _addr, _type_mask_ptr) = Heap::StaticExpression(head, n);
		_end = _addr + n;
	}

	inline direct_storage &operator<<(const BaseExpressionRef &expr) {
		assert(_addr < _end);
		_type_mask |= expr->base_type_mask();
		*_addr++ = expr;
		return *this;
	}

	inline direct_storage &operator<<(BaseExpressionRef &&expr) {
		assert(_addr < _end);
		_type_mask |= expr->base_type_mask();
		*_addr++ = expr;
		return *this;
	}

	inline ExpressionRef to_expression() {
		*_type_mask_ptr = _type_mask;
		return _expr;
	}
};

template<typename F>
inline ExpressionRef tiny_expression(const BaseExpressionRef &head, const F &generate, size_t N) {
	direct_storage storage(head, N);
	generate(storage);
	return storage.to_expression();
}

template<typename F>
inline ExpressionRef expression(const BaseExpressionRef &head, const F &generate, size_t size) {
	if (size <= MaxStaticSliceSize) {
		return tiny_expression(head, generate, size);
	} else {
		heap_storage storage(size);
		generate(storage);
		return storage.to_expression(head);
	}
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

	// check against the (possibly inexact) type mask to exit early.
	// if no decision can be made, do not compute the type mask here,
	// as this corresponds to iterating all leaves, which we do anyway.
	if ((type_mask & slice.type_mask()) != 0) {

		/*if (_extent.use_count() == 1) {
			// FIXME. optimize this case. we do not need to copy here.
		}*/

		for (size_t i0 = begin; i0 < end; i0++) {
			const auto &leaf = slice[i0];

			if ((leaf->base_type_mask() & type_mask) == 0) {
				continue;
			}

			const auto leaf0 = f(leaf);

			if (leaf0) { // copy is needed now
				const size_t size = slice.size();

				auto generate_leaves = [i0, end, size, type_mask, &slice, &f, &leaf0] (auto &storage) {
					for (size_t j = 0; j < i0; j++) {
						storage << slice[j];
					}

					storage << leaf0; // leaves[i0]

					for (size_t j = i0 + 1; j < end; j++) {
						const auto &old_leaf = slice[j];

						if ((old_leaf->base_type_mask() & type_mask) == 0) {
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

				return expression(head, generate_leaves, size);
			}
		}
	}

	if (apply_head) {
		return expression(head, slice);
	} else {
		return ExpressionRef();
	}
}

typedef Slice GenericSlice;

template<typename Slice, typename Hold>
BaseExpressionRef evaluate(
	const Expression *self,
	const BaseExpressionRef &head,
	const GenericSlice &generic_slice,
	const Evaluation &evaluation) {

	if (!Hold::do_eval) {
		// no more evaluation is applied
		return BaseExpressionRef();
	}

	const Slice &slice = static_cast<const Slice&>(generic_slice);

	const Symbol *head_symbol = static_cast<const Symbol*>(head.get());

	const eval_range eval_leaf(Hold::eval(slice));

	assert(0 <= eval_leaf.first);
	assert(eval_leaf.first <= eval_leaf.second);
	assert(eval_leaf.second <= slice.size());

	const ExpressionRef intermediate_form = apply(
		head,
		slice,
		eval_leaf.first,
		eval_leaf.second,
		[&evaluation](const BaseExpressionRef &leaf) {
			return leaf->evaluate(evaluation);
		},
		head != self->_head,
		MakeTypeMask(ExpressionType) | MakeTypeMask(SymbolType));

	if (false) { // debug
		std::cout
			<< "EVALUATED " << self
			<< " AT [" << eval_leaf.first << ", " << eval_leaf.second << "]"
			<< " TO " << intermediate_form << std::endl;
	}

	const Expression *safe_intermediate_form =
		intermediate_form ?
			intermediate_form.get() :
            self;

	// Step 3
	// Apply UpValues for leaves
	// TODO

	// Step 4
	// Evaluate the head with leaves. (DownValue)

	for (const RuleRef &rule : head_symbol->down_rules[Slice::code()]) {
		const BaseExpressionRef result = rule->try_apply(safe_intermediate_form, evaluation);
		if (result) {
			return result;
		}
	}

	return intermediate_form;
}

class Evaluate {
private:
	Evaluator _vtable[NumberOfSliceCodes];

public:
	template<typename Hold, int N>
	void initialize_static_slice() {
		_vtable[StaticSlice0Code + N] = ::evaluate<StaticSlice<N>, Hold>;

		STATIC_IF (N >= 1) {
			initialize_static_slice<Hold, N-1>();
		} STATIC_ENDIF
	}

	template<typename Hold>
	void initialize() {
		static_assert(1 + PackedSliceStringCode - StaticSlice0Code == NumberOfSliceCodes, "slice code ids error");

		_vtable[DynamicSliceCode] = ::evaluate<DynamicSlice, Hold>;

		_vtable[PackedSliceMachineIntegerCode] = ::evaluate<PackedSlice<machine_integer_t>, Hold>;
		_vtable[PackedSliceMachineRealCode] = ::evaluate<PackedSlice<machine_real_t>, Hold>;
		_vtable[PackedSliceBigIntegerCode] = ::evaluate<PackedSlice<mpz_class>, Hold>;
		_vtable[PackedSliceRationalCode] = ::evaluate<PackedSlice<mpq_class>, Hold>;
		_vtable[PackedSliceStringCode] = ::evaluate<PackedSlice<std::string>, Hold>;

		initialize_static_slice<Hold, MaxStaticSliceSize>();
	}

	inline BaseExpressionRef operator()(
		const Expression *self,
		const BaseExpressionRef &head,
		SliceCode slice_code,
		const Slice &slice,
		const Evaluation &evaluation) const {

		return _vtable[slice_code](self, head, slice, evaluation);
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

	static const Evaluate *pick(Attributes attributes);
};

#endif //CMATHICS_EVALUATE_H
