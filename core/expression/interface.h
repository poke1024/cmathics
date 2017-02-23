#pragma once

#include "../slice/method.h"

class Expression : public BaseExpression {
private:
	mutable CachedCacheRef m_cache;
    const Symbol * const m_lookup_name;

protected:
	template<SliceMethodOptimizeTarget Optimize, typename R, typename F>
	friend class SliceMethod;

	friend class ArraySlice;
	friend class VCallSlice;

	friend class SlowLeafSequence;

	const Slice * const _slice_ptr;

	virtual BaseExpressionRef materialize_leaf(size_t i) const = 0;

	virtual TypeMask materialize_type_mask() const = 0;

    virtual TypeMask materialize_exact_type_mask() const = 0;

 public:
    static constexpr Type Type = ExpressionType;

	const BaseExpressionRef _head;

	inline Expression(const BaseExpressionRef &head, SliceCode slice_id, const Slice *slice_ptr) :
		BaseExpression(build_extended_type(ExpressionType, slice_id)),
        m_lookup_name(head->lookup_name()),
		_head(head),
		_slice_ptr(slice_ptr) {
	}

    inline const Symbol *lookup_name() const {
        return m_lookup_name;
    }

    template<size_t N>
	inline const BaseExpressionRef *n_leaves() const;

	inline SliceCode slice_code() const {
		return SliceCode(extended_type_info(_extended_type));
	}

	inline size_t size() const {
		return _slice_ptr->m_size;
	}

	inline BaseExpressionRef leaf(size_t i) const;

	template<SliceCode StaticSliceCode = SliceCode::Unknown, typename F>
	inline auto with_leaves_array(const F &f) const {
		const BaseExpressionRef * const leaves = _slice_ptr->m_address;

		// the check with slice_needs_no_materialize() here is really just an additional
		// optimization that allows the compiler to reduce this code to the first case
		// alone if it's statically clear that we're always dealing with a non-packed
		// slice.

		if (slice_needs_no_materialize(StaticSliceCode) || leaves) {
            assert(leaves);
			return f(leaves, size());
		} else {
			UnsafeBaseExpressionRef materialized;
			return f(materialize(materialized), size());
		}
	}

	template<SliceMethodOptimizeTarget Optimize, typename F>
	inline auto with_slice_implementation(const F &f) const;

	template<SliceMethodOptimizeTarget Optimize, typename F>
	inline auto with_slice_implementation(F &f) const;

	template<typename F>
	inline auto with_slice(const F &f) const {
		return with_slice_implementation<DoNotCompileToSliceType, F>(f);
	}

	template<typename F>
	inline auto with_slice(F &f) const {
		return with_slice_implementation<DoNotCompileToSliceType, F>(f);
	}

	template<typename F>
	inline auto with_slice_c(const F &f) const {
		return with_slice_implementation<CompileToSliceType, F>(f);
	}

	template<typename F>
	inline auto with_slice_c(F &f) const {
		return with_slice_implementation<CompileToSliceType, F>(f);
	}

	template<typename F>
	inline auto map(const BaseExpressionRef &head, const F &f) const;

	template<typename F>
	inline auto parallel_map(const BaseExpressionRef &head, const F &f) const;

	virtual const BaseExpressionRef *materialize(UnsafeBaseExpressionRef &materialized) const = 0;

	virtual inline BaseExpressionPtr head(const Symbols &symbols) const final {
		return _head.get();
	}

    inline BaseExpressionPtr head() const {
        return _head.get();
    }

	BaseExpressionRef evaluate_expression(
		const Evaluation &evaluation) const;

	virtual BaseExpressionRef evaluate_expression_with_non_symbol_head(
		const Evaluation &evaluation) const = 0;

	virtual ExpressionRef slice(const BaseExpressionRef &head, index_t begin, index_t end = INDEX_MAX) const = 0;

	inline CacheRef get_cache() const { // concurrent.
		return m_cache;
	}

	inline CacheRef ensure_cache() const { // concurrent.
        return CacheRef(m_cache.ensure([] () {
            return Pool::new_cache();
        }));
	}

    virtual SymbolicFormRef instantiate_symbolic_form() const;

	void symbolic_initialize(
		const std::function<SymEngineRef()> &f,
		const Evaluation &evaluation) const;

	BaseExpressionRef symbolic_evaluate_unary(
		const SymEngineUnaryFunction &f,
		const Evaluation &evaluation) const;

	BaseExpressionRef symbolic_evaluate_binary(
		const SymEngineBinaryFunction &f,
		const Evaluation &evaluation) const;

	virtual MatchSize leaf_match_size() const = 0;

	inline PatternMatcherRef expression_matcher() const;

	inline PatternMatcherRef string_matcher() const;

	virtual std::tuple<bool, UnsafeExpressionRef> thread(const Evaluation &evaluation) const = 0;

	inline ExpressionRef flatten_sequence() const;

    inline ExpressionRef flatten_sequence_or_copy() const;
};

#include "../slice/method.tcc"

template<SliceMethodOptimizeTarget Optimize, typename F>
inline auto Expression::with_slice_implementation(const F &f) const {
    // we deduce F's return type by asking that F returns if we pass in BigSlice. note that this is
    // just for return type deduction; we could pass any slice type in here for compile time analysis.
    using R = typename std::result_of<F(const BigSlice&)>::type;
    static const SliceMethod<Optimize, R, decltype(lambda(f))> method;
    return method(lambda(f), this);
}

template<SliceMethodOptimizeTarget Optimize, typename F>
inline auto Expression::with_slice_implementation(F &f) const { // see above
    using R = typename std::result_of<F(const BigSlice&)>::type;
    static const SliceMethod<Optimize, R, decltype(lambda(f))> method;
    return method(lambda(f), this);
}

template<typename F>
inline auto Expression::map(const BaseExpressionRef &head, const F &f) const {
    return with_slice_c([&f, &head] (const auto &slice) {
        return ExpressionRef(expression(head, slice.map(f)));
    });
}

template<typename F>
inline auto Expression::parallel_map(const BaseExpressionRef &head, const F &f) const {
    return with_slice_c([&f, &head] (const auto &slice) {
        return ExpressionRef(expression(head, slice.parallel_map(f)));
    });
}

inline BaseExpressionRef Expression::leaf(size_t i) const {
    return with_slice([i] (const auto &slice) {
        return slice[i];
    });
}
