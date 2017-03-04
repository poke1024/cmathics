#pragma once

template<TypeMask type_mask, typename Slice, typename FReference>
class map_base {
protected:
	const BaseExpressionRef &m_head;
	const bool m_is_new_head;
	const Slice &m_slice;
	const size_t m_begin;
	const size_t m_end;
	const FReference m_f;

	inline ExpressionRef keep() const {
		if (m_is_new_head) {
			return expression(m_head, Slice(m_slice));
		} else {
			return ExpressionRef();
		}
	}

public:
	inline map_base(
		const BaseExpressionRef &head,
		const bool is_new_head,
		const FReference f,
		const Slice &slice,
		const size_t begin,
		const size_t end) :

		m_head(head),
		m_is_new_head(is_new_head),
		m_f(f),
		m_slice(slice),
		m_begin(begin),
		m_end(end) {
	}
};

template<TypeMask TTypeMask, typename Slice, typename FReference>
class sequential_map : public map_base<TTypeMask, Slice, FReference> {
protected:
	using base = map_base<TTypeMask, Slice, FReference>;

public:
	using base::map_base;

	inline ExpressionRef copy(
		size_t first_index,
		const BaseExpressionRef &first_leaf) const {

		return expression(base::m_head, sequential(
			[this, first_index, &first_leaf] (auto &store) {
				const Slice &slice = base::m_slice;
				const size_t size = slice.size();

				const size_t begin = first_index;
				const size_t end = base::m_end;

				const FReference f = base::m_f;

				for (size_t j = 0; j < begin; j++) {
					store(BaseExpressionRef(slice[j]));
				}

				store(BaseExpressionRef(first_leaf));

				for (size_t j = begin + 1; j < end; j++) {
					BaseExpressionRef old_leaf = slice[j];

					if ((old_leaf->type_mask() & TTypeMask) == 0) {
						store(std::move(old_leaf));
					} else {
						BaseExpressionRef new_leaf = f(j, old_leaf);

						if (new_leaf) {
							store(std::move(new_leaf));
						} else {
							store(std::move(old_leaf));
						}
					}
				}

				for (size_t j = end; j < size; j++) {
					store(BaseExpressionRef(slice[j]));
				}
			}, base::m_slice.size()));
	}

	inline ExpressionRef operator()() const {
		const Slice &slice = base::m_slice;

		if (TTypeMask != UnknownTypeMask && (TTypeMask & slice.type_mask()) == 0) {
			return base::keep();
		}

		const size_t begin = base::m_begin;
		const size_t end = base::m_end;
		const FReference f = base::m_f;

		for (size_t i = begin; i < end; i++) {
			const auto leaf = slice[i];

			if ((leaf->type_mask() & TTypeMask) == 0) {
				continue;
			}

			const BaseExpressionRef result = f(i, leaf);

			if (result) {
				return copy(i, result);
			}
		}

		return base::keep();
	}
};

template<TypeMask type_mask, typename Slice, typename FReference>
class parallel_map : public map_base<type_mask, Slice, FReference> {
protected:
    using base = map_base<type_mask, Slice, FReference>;

public:
    using base::map_base;

    inline ExpressionRef operator()() const {
        const Slice &slice = base::m_slice;

        if (type_mask != UnknownTypeMask && (type_mask & slice.type_mask()) == 0) {
            return base::keep();
        }

        const size_t begin = base::m_begin;
        const size_t end = base::m_end;
        const FReference f = base::m_f;

        TemporaryRefVector v;

        std::atomic_flag lock = ATOMIC_FLAG_INIT;
        bool changed = false;
        TypeMask new_type_mask = 0;

        parallelize([begin, end, &slice, &f, &v, &lock, &changed, &new_type_mask] (size_t i) {
            const size_t k = begin + i;
            BaseExpressionRef leaf = f(k, slice[k]);
            if (leaf) {
                while (lock.test_and_set() == 1) {
                    std::this_thread::yield();
                }
                if (!changed) {
                    try {
                        v.resize(end - begin);
                    } catch(...) {
                        lock.clear();
                        throw;
                    }
                    changed = true;
                }
                new_type_mask |= leaf->type_mask();
                v[i] = std::move(leaf);
                lock.clear();
            }
        }, end - begin);

        if (changed) {
            if (begin == 0 && end == slice.size()) {
                return expression(base::m_head, parallel([&v, &slice] (size_t i) {
                    const BaseExpressionRef &leaf = v[i];
                    return leaf ? leaf : slice[i];
                }, slice.size()));
            } else {
                return expression(base::m_head, parallel([begin, end, &v, &slice] (size_t i) {
                    if (i < begin || i >= end) {
                        return slice[i];
                    } else {
                        const BaseExpressionRef &leaf = v[i - begin];
                        return leaf ? leaf : slice[i];
                    }
                }, slice.size()));
            }
        } else {
            return base::keep();
        }
    }
};

inline bool check_parallelize(bool parallelize) {
    return parallelize;
}

inline bool check_parallelize(const Evaluation &evaluation) {
    return evaluation.parallelize;
}

template<TypeMask T, typename F, typename Slice, typename Parallelize>
inline ExpressionRef apply_conditional_map_indexed(
	const BaseExpressionRef &head,
	const bool is_new_head,
	const F &f,
	const Slice &slice,
	const size_t begin,
	const size_t end,
    const Parallelize &parallelize) {

#if FASTER_COMPILE
	const std::function<BaseExpressionRef(size_t, const BaseExpressionRef&)> generic_f = f.lambda;

	if (!check_parallelize(parallelize)) {
		const sequential_map<T, Slice, decltype(generic_f)> map(
			head, is_new_head, generic_f, slice, begin, end);
		return map();
	} else {
		const parallel_map<T, Slice, decltype(generic_f)> map(
			head, is_new_head, generic_f, slice, begin, end);
		return map();
	}
#else
	if (!check_parallelize(parallelize)) {
		const sequential_map<T, Slice, typename std::add_lvalue_reference<decltype(f.lambda)>::type> map(
			head, is_new_head, f.lambda, slice, begin, end);
		return map();
	} else {
        const parallel_map<T, Slice, typename std::add_lvalue_reference<decltype(f.lambda)>::type> map(
            head, is_new_head, f.lambda, slice, begin, end);
        return map();
	}
#endif
}

template<TypeMask T, typename F, typename Slice, typename Parallelize>
inline ExpressionRef apply_conditional_map(
	const BaseExpressionRef &head,
	const bool is_new_head,
	const F &f,
    const Slice &slice,
    const size_t begin,
	const size_t end,
	const Parallelize &parallelize) {

	return apply_conditional_map_indexed<T>(
		head, is_new_head,
		lambda([&f] (size_t i, const BaseExpressionRef &leaf) -> BaseExpressionRef {
			return f.lambda(leaf);
		}), slice, begin, end, parallelize);
}

template<TypeMask T, Type T1, Type... Types, typename... Args>
inline auto apply_conditional_map(const Args&... args) {
	return apply_conditional_map<T | make_type_mask(T1), Types...>(args...);
}

template<Type... Types, typename... Args>
inline auto conditional_map(const Args&... args) {
	return apply_conditional_map<0, Types...>(args...);
}

template<typename... Args>
inline auto conditional_map_all(const Args&... args) {
	return apply_conditional_map<UnknownTypeMask>(args...);
}

template<TypeMask T, Type T1, Type... Types, typename... Args>
inline auto apply_conditional_map_indexed(const Args&... args) {
	return apply_conditional_map_indexed<T | make_type_mask(T1), Types...>(args...);
}

template<Type... Types, typename... Args>
inline auto conditional_map_indexed(const Args&... args) {
	return apply_conditional_map_indexed<0, Types...>(args...);
}

template<typename... Args>
inline auto conditional_map_indexed_all(const Args&... args) {
	return apply_conditional_map_indexed<UnknownTypeMask>(args...);
}

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
