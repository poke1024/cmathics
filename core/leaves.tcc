#include "primitives.tcc"
#include "heap.tcc"
#include <static_if/static_if.hpp>

template<typename T>
class BaseExpressionToPrimitive {
public:
	inline T convert(const BaseExpressionRef &u) const {
		return to_primitive<T>(u);
	}
};

template<typename T>
class PrimitiveToBaseExpression {
public:
	inline BaseExpressionRef convert(const T &u) const {
		return from_primitive(u);
	}
};

template<typename U>
inline BaseExpressionRef PackedSlice<U>::operator[](size_t i) const {
	return from_primitive(_begin[i]);
}

template<typename R, typename F>
class SliceMethod<CompileToSliceType, R, F> {
private:
	std::function<R(const F&, const Expression *)> m_implementations[NumberOfSliceCodes];

	template<SliceCode slice_code>
	void init_static_slice() {
		m_implementations[slice_code] = [] (const F &f, const Expression *expr) {
			return f(*static_cast<const StaticSlice<slice_code - StaticSlice0Code>*>(expr->_slice_ptr));
		};
		STATIC_IF (slice_code < StaticSliceNCode) {
			init_static_slice<SliceCode(slice_code + 1)>();
		} STATIC_ENDIF
	}

	template<SliceCode slice_code>
	void init_packed_slice() {
		m_implementations[slice_code] = [] (const F &f, const Expression *expr) {
			return f(*static_cast<const PackedSlice<typename PackedSliceType<slice_code>::type>*>(expr->_slice_ptr));
		};
		STATIC_IF (slice_code < PackedSliceNCode) {
			init_packed_slice<SliceCode(slice_code + 1)>();
		} STATIC_ENDIF
	}

public:
	SliceMethod() {
		init_static_slice<StaticSlice0Code>();

		m_implementations[DynamicSliceCode] = [] (const F &f, const Expression *expr) {
			return f(*static_cast<const DynamicSlice*>(expr->_slice_ptr));
		};

		init_packed_slice<PackedSlice0Code>();
	}

	inline R operator()(const F &f, const Expression *expr) const {
		return m_implementations[expr->slice_code()](f, expr);
	}
};

class ArraySlice {
private:
	const BaseExpressionRef * const m_leaves;
	const Expression * const m_expr;

public:
	inline ArraySlice(const BaseExpressionRef *leaves, const Expression *expr) : m_leaves(leaves), m_expr(expr) {
	}

	inline const BaseExpressionRef &operator[](size_t i) const {
		return m_leaves[i];
	}

	inline size_t size() const {
		return m_expr->size();
	}

	inline TypeMask type_mask() const {
		return m_expr->materialize_type_mask();
	}

	inline ExpressionRef clone(const BaseExpressionRef &head) const {
		return m_expr->clone(head);
	}
};

template<typename R, typename F>
class SliceMethod<CompileToPackedSliceType, R, F> {
private:
	std::function<R(const F&, const Expression *)> m_implementations[1 + PackedSliceNCode - PackedSlice0Code];

	template<SliceCode slice_code>
	void init_packed_slice() {
		m_implementations[slice_code - PackedSlice0Code] = [] (const F &f, const Expression *expr) {
			return f(*static_cast<const PackedSlice<typename PackedSliceType<slice_code>::type>*>(expr->_slice_ptr));
		};
		STATIC_IF (slice_code < PackedSliceNCode) {
			init_packed_slice<SliceCode(slice_code + 1)>();
		} STATIC_ENDIF
	}

public:
	SliceMethod() {
		init_packed_slice<PackedSlice0Code>();
	}

	inline R operator()(const F &f, const Expression *expr) const {
		const BaseExpressionRef * const leaves = expr->_slice_ptr->_address;
		if (leaves) {
			const ArraySlice slice(leaves, expr);
			return f(slice);
		} else {
			const SliceCode slice_code = expr->slice_code();
			assert(is_packed_slice(slice_code));
			return m_implementations[slice_code - PackedSlice0Code](f, expr);
		}
	}
};

class VCallSlice {
private:
	const Expression * const m_expr;

public:
	inline VCallSlice(const Expression *expr) : m_expr(expr) {
	}

	inline const BaseExpressionRef operator[](size_t i) const {
		return m_expr->materialize_leaf(i);
	}

	inline size_t size() const {
		return m_expr->size();
	}

	inline TypeMask type_mask() const {
		return m_expr->materialize_type_mask();
	}

	inline ExpressionRef clone(const BaseExpressionRef &head) const {
		return m_expr->clone(head);
	}
};

template<typename R, typename F>
class SliceMethod<DoNotCompileToSliceType, R, F> {
public:
	SliceMethod() {
	}

	inline R operator()(const F &f, const Expression *expr) const {
		const BaseExpressionRef * const leaves = expr->_slice_ptr->_address;
		if (leaves) {
			const ArraySlice slice(leaves, expr);
			return f(slice);
		} else {
			const VCallSlice slice(expr);
			return f(slice);
		}
	}
};

template<SliceMethodOptimizeTarget Optimize, typename F>
inline auto Expression::with_slice(const F &f) const {
	// we deduce F's return type by asking that F returns if we pass in DynamicSlice. note that this is
	// just for return type deduction; we could pass any slice type in here for compile time analysis.
	using R = typename std::result_of<F(const DynamicSlice&)>::type;

	static const SliceMethod<Optimize, R, F> method;
	return method(f, this);
}

template<typename F>
inline auto Expression::map(const BaseExpressionRef &head, const F &f) const {
	return with_slice<CompileToSliceType>([&f, &head] (const auto &slice) {
		return ExpressionRef(expression(head, slice.map(f)));
	});
}

template<typename F>
inline auto Expression::parallel_map(const BaseExpressionRef &head, const F &f) const {
	return with_slice<CompileToSliceType>([&f, &head] (const auto &slice) {
		return ExpressionRef(expression(head, slice.parallel_map(f)));
	});
}

inline BaseExpressionRef Expression::leaf(size_t i) const {
	return with_slice([i] (const auto &slice) {
		return slice[i];
	});
}
