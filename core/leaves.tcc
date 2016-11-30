#include "primitives.tcc"
#include "heap.tcc"

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
