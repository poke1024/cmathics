#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <vector>

#include "types.h"
#include "expression.h"
#include "pattern.h"
#include "integer.h"
#include "arithmetic.h"
#include "real.h"
#include "rational.h"
#include "primitives.h"

BaseExpressionRef Plus3Rule::try_apply(
	const Expression *expr, const Evaluation &evaluation) const {

	return expr->Plus();
}

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

	constexpr TypeMask rational_mask = MakeTypeMask(RationalType);
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
