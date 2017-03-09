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
	const Evaluation &m_evaluation;

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
		const size_t end,
		const Evaluation &evaluation) :

		m_head(head),
		m_is_new_head(is_new_head),
		m_f(f),
		m_slice(slice),
		m_begin(begin),
		m_end(end),
		m_evaluation(evaluation) {
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
		}, end - begin, base::m_evaluation);

		if (changed) {
			if (begin == 0 && end == slice.size()) {
				return expression(base::m_head, parallel([&v, &slice] (size_t i) {
					const BaseExpressionRef &leaf = v[i];
					return leaf ? leaf : slice[i];
				}, slice.size(), base::m_evaluation));
			} else {
				return expression(base::m_head, parallel([begin, end, &v, &slice] (size_t i) {
					if (i < begin || i >= end) {
						return slice[i];
					} else {
						const BaseExpressionRef &leaf = v[i - begin];
						return leaf ? leaf : slice[i];
					}
				}, slice.size(), base::m_evaluation));
			}
		} else {
			return base::keep();
		}
	}
};

inline conditional_map_head keep_head(const BaseExpressionRef &head) {
	return conditional_map_head{head, false};
}

inline conditional_map_head replace_head(const BaseExpressionRef &head) {
	return conditional_map_head{head, true};
}

inline conditional_map_head replace_head(
	const BaseExpressionRef &old_head, const BaseExpressionRef &new_head) {

	if (new_head && new_head.get() != old_head.get()) {
		return replace_head(new_head);
	} else {
		return keep_head(old_head);
	}
}

template<TypeMask T = UnknownTypeMask, typename F, typename Slice>
inline ExpressionRef conditional_map_indexed(
	const conditional_map_head &head,
	const F &f,
	const Slice &slice,
	const size_t begin,
	const size_t end,
	const Evaluation &evaluation) {

#if FASTER_COMPILE
	const std::function<BaseExpressionRef(size_t, const BaseExpressionRef&)> generic_f = f.lambda;

	if (!evaluation.parallelize) {
		const sequential_map<T, Slice, decltype(generic_f)> map(
			head.head, head.is_new_head, generic_f, slice, begin, end, evaluation);
		return map();
	} else {
		const parallel_map<T, Slice, decltype(generic_f)> map(
			head.head, head.is_new_head, generic_f, slice, begin, end, evaluation);
		return map();
	}
#else
	if (!!evaluation.parallelize) {
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

template<TypeMask T = UnknownTypeMask, typename F, typename Slice>
inline ExpressionRef conditional_map_indexed(
	const conditional_map_head &head,
	const F &f,
	const Slice &slice,
	const Evaluation &evaluation) {

	return conditional_map_indexed(head, f, slice, 0, slice.size(), evaluation);
}

template<TypeMask T = UnknownTypeMask, typename F, typename Slice>
inline ExpressionRef conditional_map(
	const conditional_map_head &head,
	const F &f,
	const Slice &slice,
	const size_t begin,
	const size_t end,
	const Evaluation &evaluation) {

	return conditional_map_indexed<T>(
		head,
		lambda([&f] (size_t i, const BaseExpressionRef &leaf) -> BaseExpressionRef {
			return f.lambda(leaf);
		}), slice, begin, end, evaluation);
}

template<TypeMask T = UnknownTypeMask, typename F, typename Slice>
inline ExpressionRef conditional_map(
	const conditional_map_head &head,
	const F &f,
	const Slice &slice,
	const Evaluation &evaluation) {

	return conditional_map(head, f, slice, 0, slice.size(), evaluation);
}

template<TypeMask T, Type T1, Type... Types, typename... Args>
inline auto conditional_map(const Args&... args) {
	return conditional_map<T | make_type_mask(T1), Types...>(args...);
}

template<Type... Types, typename... Args>
inline auto selective_conditional_map(const Args&... args) {
	return conditional_map<0, Types...>(args...);
}

template<TypeMask T, Type T1, Type... Types, typename... Args>
inline auto conditional_map_indexed(const Args&... args) {
	return conditional_map_indexed<T | make_type_mask(T1), Types...>(args...);
}

template<Type... Types, typename... Args>
inline auto selective_conditional_map_indexed(const Args&... args) {
	return conditional_map_indexed<0, Types...>(args...);
}
