#pragma once

#include "map.h"

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
		void initialize_static_slice();

		Precompiled();

		inline const Evaluate *vtable() const {
			return &m_vtable;
		}
	};

private:
    enum {
        PrecompiledNone = 0,
        PrecompiledHoldFirst,
        PrecompiledHoldRest,
        PrecompiledHoldAll,
        PrecompiledHoldAllComplete,
        PrecompiledListableNumericFunction,
        PrecompiledDynamic,
        NumPrecompiledVariants
    };

    const Evaluate *m_evaluate[NumPrecompiledVariants];

protected:
    EvaluateDispatch();

public:
	static void init();

	static DispatchableAttributes pick(Attributes attributes);

    static inline BaseExpressionRef call(
        DispatchableAttributes id,
        const Symbol *symbol,
        const Expression *expr,
        SliceCode slice_code,
        const Slice &slice,
        const Evaluation &evaluation) {

        const Evaluate *evaluate = s_instance->m_evaluate[id & 0xff];

        return (*evaluate)(
            expr,
            BaseExpressionRef(symbol),
            slice_code,
            slice,
            Attributes(id >> 8),
            evaluation);

    }
};

inline BaseExpressionRef SymbolState::dispatch(
	const Expression *expr,
	SliceCode slice_code,
	const Slice &slice,
	const Evaluation &evaluation) const {

    return EvaluateDispatch::call(
        m_dispatch,
        m_symbol,
        expr,
        slice_code,
        slice,
        evaluation);
}
