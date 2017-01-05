#ifndef CMATHICS_EVALUATE_H
#define CMATHICS_EVALUATE_H

#include "leaves.h"

inline ExpressionRef heap_storage::to_expression(const BaseExpressionRef &head) {
	return expression(head, std::move(_leaves), _type_mask);
}

template<typename F>
inline ExpressionRef expression_from_generator(const BaseExpressionRef &head, const F &generate) {
	heap_storage storage;
	generate(storage);
	return storage.to_expression(head);
}

template<typename F, typename T>
inline ExpressionRef expression_from_generator(const BaseExpressionRef &head, const F &generate, size_t size, T &r) {
	heap_storage storage(size);
	r = generate(storage);
	return storage.to_expression(head);
}

class direct_storage {
protected:
	UnsafeExpressionRef _expr;
	UnsafeBaseExpressionRef *_addr;
	UnsafeBaseExpressionRef *_end;
	TypeMask _type_mask;
	std::atomic<TypeMask> *_type_mask_ptr;

public:
	inline direct_storage(const BaseExpressionRef &head, size_t n) : _type_mask(0) {
		std::tie(_expr, _addr, _type_mask_ptr) = Pool::StaticExpression(head, n);
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

	inline UnsafeExpressionRef to_expression() {
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
inline ExpressionRef expression_from_generator(const BaseExpressionRef &head, const F &generate, size_t size) {
	if (size <= MaxStaticSliceSize) {
		return tiny_expression(head, generate, size);
	} else {
		heap_storage storage(size);
		generate(storage);
		return storage.to_expression(head);
	}
}

template<typename Slice>
struct transform_utils {
	template<typename T, typename F>
	static inline ExpressionRef generate(const BaseExpressionRef &head, const F &generate, size_t size, T &state) {
		return expression(head, Slice::create(generate, size, state));
	}

	static inline ExpressionRef clone(const BaseExpressionRef &head, const Slice &slice) {
		return expression(head, Slice(slice));
	}
};

template<>
struct transform_utils<ArraySlice> {
	template<typename T, typename F>
	static inline ExpressionRef generate(const BaseExpressionRef &head, const F &generate, size_t size, T &state) {
		return expression_from_generator(head, generate, size, state);
	}

	static inline ExpressionRef clone(const BaseExpressionRef &head, const ArraySlice &slice) {
		return slice.clone(head);
	}
};

template<>
struct transform_utils<VCallSlice> {
	template<typename T, typename F>
	static inline ExpressionRef generate(const BaseExpressionRef &head, const F &generate, size_t size, T &state) {
		return expression_from_generator(head, generate, size, state);
	}

	static inline ExpressionRef clone(const BaseExpressionRef &head, const VCallSlice &slice) {
		return slice.clone(head);
	}
};

template<TypeMask Types = UnknownTypeMask, typename Slice, typename T, typename F>
std::tuple<ExpressionRef, T> transform(
	const BaseExpressionRef &head,
	const Slice &slice,
	const size_t begin,
	const size_t end,
	const F &f,
	const T initial_state,
	const bool apply_head) {

	T state = initial_state;

	// check against the (possibly inexact) type mask to exit early.
	// if no decision can be made, do not compute the type mask here,
	// as this corresponds to iterating all leaves, which we do anyway.
	if (Types == UnknownTypeMask || (Types & slice.type_mask()) != 0) {

		for (size_t i0 = begin; i0 < end; i0++) {
			const auto leaf = slice[i0];

			if ((leaf->base_type_mask() & Types) == 0) {
				continue;
			}

			const std::tuple<BaseExpressionRef, T> result = f(i0, leaf, state);
			state = std::get<1>(result);

			if (std::get<0>(result)) { // copy is needed now
				const size_t size = slice.size();

				auto generate = [i0, end, size, &slice, &f, &result, &state] (auto &storage) {
					for (size_t j = 0; j < i0; j++) {
						storage << slice[j];
					}

					storage << std::get<0>(result); // leaves[i0]

					T inner_state = state;
					for (size_t j = i0 + 1; j < end; j++) {
						const auto old_leaf = slice[j];

						if ((old_leaf->base_type_mask() & Types) == 0) {
							storage << old_leaf;
						} else {
							std::tuple<BaseExpressionRef, T> inner_result = f(j, old_leaf, inner_state);
							inner_state = std::get<1>(inner_result);

							if (std::get<0>(inner_result)) {
								storage << std::move(std::get<0>(inner_result));
							} else {
								storage << old_leaf;
							}
						}
					}

					for (size_t j = end; j < size; j++) {
						storage << slice[j];
					}

					return inner_state;
				};

				return std::make_tuple(transform_utils<Slice>::generate(
					head, generate, size, state), state);
			}
		}
	}

	if (apply_head) {
		return std::make_tuple(transform_utils<Slice>::clone(head, slice), state);
	} else {
		return std::make_tuple(ExpressionRef(), state);
	}
}

template<TypeMask Types, typename Slice, typename F>
ExpressionRef transform(
	const BaseExpressionRef &head,
	const Slice &slice,
	const size_t begin,
	const size_t end,
	const F &f,
	const bool apply_head) {

	return std::get<0>(transform<Types, Slice, nothing>(
		head, slice, begin, end, [&f] (size_t, const BaseExpressionRef &leaf, nothing) {
			return std::make_tuple(f(leaf), nothing());
	}, nothing(), apply_head));
};


typedef Slice GenericSlice;

typedef std::pair<size_t, size_t> eval_range;

template<typename Slice, typename ReducedAttributes>
BaseExpressionRef evaluate(
	const Expression *self,
	const BaseExpressionRef &head,
	const GenericSlice &generic_slice,
    const Attributes full_attributes,
	const Evaluation &evaluation) {

	const Slice &slice = static_cast<const Slice&>(generic_slice);
	eval_range eval_leaf;

	const ReducedAttributes attributes(full_attributes);

	if (attributes & Attributes::HoldAllComplete) {
		// no more evaluation is applied
		return BaseExpressionRef();
	} else if (attributes & Attributes::HoldFirst) {
		if (attributes & Attributes::HoldRest) { // i.e. HoldAll
			// TODO flatten sequences
			eval_leaf = eval_range(0, 0);
		} else {
			eval_leaf = eval_range(1, slice.size());
		}
	} else if (attributes & Attributes::HoldRest) {
		eval_leaf = eval_range(0, 1);
	} else {
		eval_leaf = eval_range(0, slice.size());
	}

	const Symbol * const head_symbol = static_cast<const Symbol*>(head.get());

	assert(0 <= eval_leaf.first);
	assert(eval_leaf.first <= eval_leaf.second);
	assert(eval_leaf.second <= slice.size());

	const ExpressionRef intermediate_form = transform<MakeTypeMask(ExpressionType) | MakeTypeMask(SymbolType)>(
		head,
		slice,
		eval_leaf.first,
		eval_leaf.second,
		[&evaluation](const BaseExpressionRef &leaf) {
			return leaf->evaluate(evaluation);
		},
		head != self->_head);

	if (false) { // debug
		std::cout
			<< "EVALUATED " << self
			<< " AT [" << eval_leaf.first << ", " << eval_leaf.second << "]"
			<< " TO " << intermediate_form << std::endl;
	}

	const Expression * const safe_intermediate_form =
		intermediate_form ?
			intermediate_form.get() :
            self;

	if (attributes & Attributes::Listable) {
		bool done;
		UnsafeExpressionRef threaded;
		std::tie(done, threaded) = safe_intermediate_form->thread(evaluation);
		if (done) {
			return threaded;
		}
	}

	const SymbolRules *rules = head_symbol->rules();

	if (rules) {
		// Step 3
		// Apply UpValues for leaves
		// TODO

		// Step 4
		// Evaluate the head with leaves. (DownValue)

		const BaseExpressionRef down_form = rules->down_rules.try_and_apply<Slice>(
				safe_intermediate_form, evaluation);
		if (down_form) {
			return down_form;
		}
	}

	// Simplify symbolic form, if possible. We guarantee that no vcall
    // happens unless a symbolic resolution is really necessary.

	SymbolicFormRef form = fast_symbolic_form(safe_intermediate_form);

	if (!form->is_none() && !form->is_simplified()) {
		if (false) { // debug
			std::cout << "inspecting " << safe_intermediate_form->fullform() << std::endl;
		}

		const BaseExpressionRef simplified = from_symbolic_form(form->get(), evaluation);

		if (false) { // debug
			std::cout << "simplified into " << simplified->fullform() << std::endl;
		}

		assert(symbolic_form(simplified)->is_simplified());

		return simplified;
	}

	/*if (!form) {
		// if this expression already has a symbolic form attached, then
		// we already simplified it earlier and we must not recheck here;
        // note that we end up in an infinite loop otherwise.

		form = ::instantiate_symbolic_form(safe_intermediate_form);

        } else {
            safe_intermediate_form->set_no_symbolic_form();
        }
	}*/

	return intermediate_form;
}

template<Attributes fixed_attributes>
class FixedAttributes {
public:
	inline FixedAttributes(Attributes attributes) {
	}

	inline constexpr bool
	operator&(Attributes y) const {
		return (attributes_bitmask_t(fixed_attributes) & attributes_bitmask_t(y)) != 0;
	}
};

using EvaluateFunction = std::function<BaseExpressionRef(
	const Expression *expr,
	const BaseExpressionRef &head,
	const Slice &slice,
	const Attributes attributes,
	const Evaluation &evaluation)>;

class Evaluate {
protected:
	friend class EvaluateDispatch;

	EvaluateFunction entry[NumberOfSliceCodes];

public:
	inline BaseExpressionRef operator()(
		const Expression *expr,
		const BaseExpressionRef &head,
		SliceCode slice_code,
		const Slice &slice,
		const Attributes attributes,
		const Evaluation &evaluation) const {

		return entry[slice_code](expr, head, slice, attributes, evaluation);
	}
};

class EvaluateDispatch {
private:
	static EvaluateDispatch *s_instance;

	template<typename ReducedAttributes>
	class Precompiled {
	private:
		Evaluate m_vtable;

	public:
		template<int N>
		void initialize_static_slice() {
			m_vtable.entry[StaticSlice0Code + N] = ::evaluate<StaticSlice<N>, ReducedAttributes>;

			STATIC_IF (N >= 1) {
				initialize_static_slice<N - 1>();
			} STATIC_ENDIF
		}

		Precompiled() {
			static_assert(1 + PackedSliceMachineRealCode - StaticSlice0Code ==
			              NumberOfSliceCodes, "slice code ids error");

			m_vtable.entry[DynamicSliceCode] = ::evaluate<DynamicSlice, ReducedAttributes>;

			m_vtable.entry[PackedSliceMachineIntegerCode] =
					::evaluate<PackedSlice<machine_integer_t>, ReducedAttributes>;
			m_vtable.entry[PackedSliceMachineRealCode] =
					::evaluate<PackedSlice<machine_real_t>, ReducedAttributes>;

			initialize_static_slice<MaxStaticSliceSize>();
		}

		inline const Evaluate *vtable() const {
			return &m_vtable;
		}
	};

private:
	Precompiled<FixedAttributes<Attributes::None>> m_none;

	Precompiled<FixedAttributes<Attributes::HoldFirst>> m_hold_first;
	Precompiled<FixedAttributes<Attributes::HoldRest>> m_hold_rest;
	Precompiled<FixedAttributes<Attributes::HoldAll>> m_hold_all;
	Precompiled<FixedAttributes<Attributes::HoldAllComplete>> m_hold_all_complete;

	Precompiled<FixedAttributes<Attributes::Listable + Attributes::NumericFunction>> m_listable_numeric_function;

	Precompiled<Attributes> m_dynamic;

public:
	static void init();

	static const Evaluate *pick(Attributes attributes);
};

inline BaseExpressionRef Symbol::dispatch(
	const Expression *expr,
	SliceCode slice_code,
	const Slice &slice,
	const Evaluation &evaluation) const {

	const Symbol * const self = this;

	return m_attributes_data.load([self, expr, slice_code, &slice, &evaluation] (auto &data, auto release) {
		const Attributes attributes = data.attributes;
		const Evaluate *const evaluate = data.evaluate;
		release();

		return (*evaluate)(
			expr,
			BaseExpressionRef(self),
			slice_code,
			slice,
			attributes,
			evaluation);
	});
}

#endif //CMATHICS_EVALUATE_H
