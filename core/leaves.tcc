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

template<typename F>
class SliceMethod {
private:
	// we deduce F's return type by asking that F returns if we pass in DynamicSlice. note that this is
	// just for return type deduction; we could pass any slice type in here for compile time analysis.
	using R = typename std::result_of<F(const DynamicSlice&)>::type;

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

	inline auto operator()(const F &f, const Expression *expr) const {
		return m_implementations[expr->slice_code()](f, expr);
	}
};

template<typename F>
inline auto Expression::with_slice(const F &f) const {
	static const SliceMethod<F> method;
	return method(f, this);
}
