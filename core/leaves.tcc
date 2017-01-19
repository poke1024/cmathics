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

#if COMPILE_TO_SLICE
template<typename R, typename F>
class SliceMethod<CompileToSliceType, R, F> {
private:
	std::function<R(const F&, const Expression *)> m_implementations[NumberOfSliceCodes];

	template<SliceCode slice_code>
	void init_static_slice() {
		m_implementations[slice_code] = [] (const F &f, const Expression *expr) {
			return f.lambda(*static_cast<const StaticSlice<slice_code - StaticSlice0Code>*>(expr->_slice_ptr));
		};
		STATIC_IF (slice_code < StaticSliceNCode) {
			init_static_slice<SliceCode(slice_code + 1)>();
		} STATIC_ENDIF
	}

	template<SliceCode slice_code>
	void init_packed_slice() {
		m_implementations[slice_code] = [] (const F &f, const Expression *expr) {
			return f.lambda(*static_cast<const PackedSlice<typename PackedSliceType<slice_code>::type>*>(expr->_slice_ptr));
		};
		STATIC_IF (slice_code < PackedSliceNCode) {
			init_packed_slice<SliceCode(slice_code + 1)>();
		} STATIC_ENDIF
	}

public:
	SliceMethod() {
		init_static_slice<StaticSlice0Code>();

		m_implementations[DynamicSliceCode] = [] (const F &f, const Expression *expr) {
			return f.lambda(*static_cast<const DynamicSlice*>(expr->_slice_ptr));
		};

		init_packed_slice<PackedSlice0Code>();
	}

	inline R operator()(const F &f, const Expression *expr) const {
		return m_implementations[expr->slice_code()](f, expr);
	}
};
#else
template<typename AccessLeaf>
class ByIndexCollection {
private:
    const AccessLeaf m_access_leaf;
	const size_t m_size;

public:
    class Iterator {
    private:
        const AccessLeaf m_access_leaf;
        size_t m_index;

    public:
        explicit Iterator(const AccessLeaf &access_leaf, size_t index) :
            m_access_leaf(access_leaf), m_index(index) {
        }

        inline auto operator*() const {
            return m_access_leaf(m_index);
        }

        inline bool operator==(const Iterator &other) const {
            return m_index == other.m_index;
        }

        inline bool operator!=(const Iterator &other) const {
            return m_index != other.m_index;
        }

        inline Iterator &operator++() {
            m_index += 1;
            return *this;
        }

        inline Iterator operator++(int) const {
            return Iterator(m_access_leaf, m_index + 1);
        }
    };

	inline ByIndexCollection(const AccessLeaf &access_leaf, size_t size) :
		m_access_leaf(access_leaf), m_size(size) {
	}

	inline Iterator begin() const {
		return Iterator(m_access_leaf, 0);
	}

	inline Iterator end() const {
		return Iterator(m_access_leaf, m_size);
	}

	inline auto operator[](size_t i) const {
		return m_access_leaf(i);
	}
};
#endif

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

#if !COMPILE_TO_SLICE
    template<typename Generate>
    inline LeafVector create(const Generate &generate, size_t n) const {
        return sequential(generate, n).vector();
    }

    inline TypeMask exact_type_mask() const {
        return m_expr->materialize_exact_type_mask();
    }

    template<typename T>
    class IndexToPrimitive {
    private:
        const BaseExpressionRef * const m_leaves;

    public:
        inline IndexToPrimitive(const BaseExpressionRef *leaves) : m_leaves(leaves) {
        }

        inline T convert(size_t i) const {
            return to_primitive<T>(m_leaves[i]);
        }
    };

    template<typename V>
    using PrimitiveCollection = PointerCollection<BaseExpressionRef, IndexToPrimitive<V>>;

    template<typename V>
    inline PrimitiveCollection<V> primitives() const {
        return PrimitiveCollection<V>(0, size(), IndexToPrimitive<V>(m_leaves));
    }

    inline auto leaves() const {
        const ExpressionRef expr(m_expr);

        const auto access_leaf = [expr] (size_t i) {
            return expr->materialize_leaf(i);
        };

        return ByIndexCollection<decltype(access_leaf)>(access_leaf, size());
    }
#endif
};

#if COMPILE_TO_SLICE
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
#endif

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

#if !COMPILE_TO_SLICE
    template<typename Generate>
    inline LeafVector create(const Generate &generate, size_t n) const {
        return sequential(generate, n).vector();
    }

    inline TypeMask exact_type_mask() const {
        return m_expr->materialize_exact_type_mask();
    }

    template<typename T>
    class IndexToPrimitive {
    private:
        const Expression * const m_expr;

    public:
        inline IndexToPrimitive(const Expression *expr) : m_expr(expr) {
        }

        inline T convert(size_t i) const {
            return to_primitive<T>(m_expr->materialize_leaf(i));
        }
    };

    template<typename V>
    using PrimitiveCollection = PointerCollection<BaseExpressionRef, IndexToPrimitive<V>>;

    template<typename V>
    inline PrimitiveCollection<V> primitives() const {
        return PrimitiveCollection<V>(0, size(), IndexToPrimitive<V>(m_expr));
    }

    inline auto leaves() const {
        const ExpressionRef expr(m_expr);

        const auto access_leaf = [expr] (size_t i) {
            return expr->materialize_leaf(i);
        };

        return ByIndexCollection<decltype(access_leaf)>(access_leaf, size());
    }
#endif
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
			return f.lambda(slice);
		} else {
			const VCallSlice slice(expr);
			return f.lambda(slice);
		}
	}
};

template<SliceMethodOptimizeTarget Optimize, typename F>
inline auto Expression::with_slice(const F &f) const {
	// we deduce F's return type by asking that F returns if we pass in DynamicSlice. note that this is
	// just for return type deduction; we could pass any slice type in here for compile time analysis.
	using R = typename std::result_of<F(const DynamicSlice&)>::type;
	static const SliceMethod<Optimize, R, decltype(lambda(f))> method;
	return method(lambda(f), this);
}

template<SliceMethodOptimizeTarget Optimize, typename F>
inline auto Expression::with_slice(F &f) const { // see above
	using R = typename std::result_of<F(const DynamicSlice&)>::type;
	static const SliceMethod<Optimize, R, decltype(lambda(f))> method;
	return method(lambda(f), this);
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
