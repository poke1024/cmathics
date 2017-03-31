#pragma once
// @formatter:off

#include "../slice/method.h"

struct conditional_map_head {
	const BaseExpressionRef &head;
	bool is_new_head;
};

class Expression : public BaseExpression {
private:
	mutable CachedCacheRef m_cache;
    mutable TaskLocalStorage<UnsafeVersionRef> m_last_evaluated;
    const Symbol * const m_lookup_name;

protected:
	template<SliceMethodOptimizeTarget Optimize, typename R, typename F>
	friend class SliceMethod;

	friend class ArraySlice;
	friend class VCallSlice;

	friend class SlowLeafSequence;

	const Slice * const _slice_ptr;

	inline BaseExpressionRef materialize_leaf(size_t i) const;

	inline TypeMask materialize_type_mask() const;

	inline TypeMask materialize_exact_type_mask() const;

    template<bool String, typename Recurse>
    inline MatchSize generic_match_size(const Recurse &recurse) const;

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

    inline bool has_leaves_array() const {
        return slice_needs_no_materialize(slice_code());
    }

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
	inline auto parallel_map(
		const BaseExpressionRef &head, const F &f, const Evaluation &evaluation) const;

	virtual inline BaseExpressionPtr head(const Symbols &symbols) const final {
		return _head.get();
	}

    inline BaseExpressionPtr head() const {
        return _head.get();
    }

	BaseExpressionRef evaluate_expression(
		const Evaluation &evaluation) const;

	inline ExpressionRef slice(const BaseExpressionRef &head, index_t begin, index_t end = INDEX_MAX) const;

	inline CacheRef get_cache() const { // concurrent.
		return m_cache;
	}

	inline CacheRef ensure_cache() const { // concurrent.
        return CacheRef(m_cache.ensure([] () {
            return Cache::construct();
        }));
	}

    virtual SymbolicFormRef instantiate_symbolic_form(const Evaluation &evaluation) const;

	void symbolic_initialize(
		const std::function<SymEngineRef()> &f,
		const Evaluation &evaluation) const;

	BaseExpressionRef symbolic_evaluate_unary(
		const SymEngineUnaryFunction &f,
		const Evaluation &evaluation) const;

	BaseExpressionRef symbolic_evaluate_binary(
		const SymEngineBinaryFunction &f,
		const Evaluation &evaluation) const;

	inline MatchSize leaf_match_size() const;

	inline PatternMatcherRef expression_matcher() const;

	inline PatternMatcherRef string_matcher() const;

	inline ExpressionRef flatten_sequence() const;

    inline ExpressionRef flatten_sequence_or_copy() const;

	virtual void sort_key(SortKey &key, const Evaluation &evaluation) const final;

	virtual void pattern_key(SortKey &key, const Evaluation &evaluation) const final;

	virtual inline bool same_indeed(const BaseExpression &item) const final;

	virtual optional<hash_t> compute_match_hash() const final;

	virtual tribool equals(const BaseExpression &item) const final;

	virtual MatchSize match_size() const final;

	virtual MatchSize string_match_size() const final;

	virtual bool is_numeric() const final;

	virtual std::string boxes_to_text(const StyleBoxOptions &options, const Evaluation &evaluation) const;

	virtual bool is_negative_introspect() const;

	virtual std::string debugform() const;

	inline BaseExpressionRef evaluate_expression_with_non_symbol_head(const Evaluation &evaluation) const;

	virtual hash_t hash() const final;

	virtual BaseExpressionRef negate(const Evaluation &evaluation) const final;

	inline std::tuple<bool, UnsafeExpressionRef> thread(const Evaluation &evaluation) const;

	template<enum Type... Types, typename F>
	inline ExpressionRef selective_conditional_map(
		const F &f, const Evaluation &evaluation) const;

	/*template<enum Type... Types, typename F>
	inline ExpressionRef selective_conditional_map(
		const conditional_map_head &head, const F &f) const;*/

	template<enum Type... Types, typename F>
	inline ExpressionRef selective_conditional_map(
		const conditional_map_head &head, const F &f, const Evaluation &evaluation) const;

	template<typename F>
	inline ExpressionRef conditional_map(
		const conditional_map_head &head, const F &f, const Evaluation &evaluation) const;

	template<typename Compute, typename Recurse>
	BaseExpressionRef do_symbolic(
		const Compute &compute,
		const Recurse &recurse,
		const Evaluation &evaluation) const;

	virtual BaseExpressionRef expand(const Evaluation &evaluation) const final;

	virtual BaseExpressionRef clone() const;

	virtual ExpressionRef clone(const BaseExpressionRef &head) const;

	virtual BaseExpressionRef replace_all(const MatchRef &match, const Evaluation &evaluation) const;

	virtual BaseExpressionRef replace_all(const ArgumentsMap &replacement, const Evaluation &evaluation) const;

	virtual BaseExpressionRef custom_format_traverse(
		const BaseExpressionRef &form,
		const Evaluation &evaluation) const;

	virtual BaseExpressionRef custom_format(
		const BaseExpressionRef &form,
		const Evaluation &evaluation) const;

	const BaseExpressionRef *materialize(UnsafeBaseExpressionRef &materialized) const;

	virtual BaseExpressionRef deverbatim() const;

    inline UnsafeVersionRef last_evaluated() const {
        return m_last_evaluated.get();
    }

    inline void set_last_evaluated(const VersionRef &version) const {
        m_last_evaluated.set(version);
    }
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
    static const SliceMethod<Optimize, R, decltype(mutable_lambda(f))> method;
    return method(mutable_lambda(f), this);
}

template<typename F>
inline auto Expression::map(const BaseExpressionRef &head, const F &f) const {
    return with_slice_c([&f, &head] (const auto &slice) {
        return ExpressionRef(expression(head, slice.map(f)));
    });
}

template<typename F>
inline auto Expression::parallel_map(
	const BaseExpressionRef &head, const F &f, const Evaluation &evaluation) const {

	return with_slice_c([&f, &head, &evaluation] (const auto &slice) {
        return ExpressionRef(expression(head, slice.parallel_map(f, evaluation)));
    });
}

inline BaseExpressionRef Expression::leaf(size_t i) const {
    return with_slice([i] (const auto &slice) {
        return slice[i];
    });
}
