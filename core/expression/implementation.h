#pragma once
// @formatter:off

#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "core/types.h"
#include "core/hash.h"
#include "core/atoms/symbol.h"
#include "core/atoms/string.h"

#include <sstream>
#include <vector>
#include <mutex>

#include <symengine/add.h>
#include <symengine/mul.h>
#include <symengine/pow.h>

template<typename Slice>
class ExpressionImplementation : public Expression, public PoolObject<ExpressionImplementation<Slice>> {
private:
	const Slice m_slice;

public:
	inline ExpressionImplementation(const BaseExpressionRef &head, const Slice &slice) :
        Expression(head, Slice::code(), &m_slice), m_slice(slice) {
		assert(head);
	}

	inline ExpressionImplementation(const BaseExpressionRef &head) :
		Expression(head, Slice::code(), &m_slice), m_slice() {
		assert(head);
	}

	template<typename F>
	inline ExpressionImplementation(const BaseExpressionRef &head, const FSGenerator<F> &generator) :
		Expression(head, Slice::code(), &m_slice), m_slice(generator) {
	}

	template<typename F>
	inline ExpressionImplementation(const BaseExpressionRef &head, const FPGenerator<F> &generator) :
		Expression(head, Slice::code(), &m_slice), m_slice(generator) {
	}

    inline const Slice &slice() const {
        return m_slice;
    }

	inline SliceCode slice_code() const { // FIXME: constexpr
		return Slice::code();
	}

	virtual const BaseExpressionRef *materialize(UnsafeBaseExpressionRef &materialized) const;
};

#include "../heap.tcc"
#include "core/atoms/numeric.tcc"
#include "construct.h"
#include "../definitions.h"
#include "../evaluation.h"
#include "../evaluate.h"
#include "../pattern/rewrite.tcc"

inline BaseExpressionRef Expression::materialize_leaf(size_t i) const {
	return with_slice([i] (const auto &slice) {
		return slice[i];
	});
}

inline TypeMask Expression::materialize_type_mask() const {
	return with_slice_c([] (const auto &slice) {
		return slice.type_mask();
	});
}

inline TypeMask Expression::materialize_exact_type_mask() const {
	return with_slice_c([] (const auto &slice) {
		return slice.exact_type_mask();
	});
}

template<Type... Types, typename F>
inline ExpressionRef Expression::conditional_map(
	const F &f,
	const Evaluation &evaluation) const {

	return with_slice_c([this, &f, &evaluation] (const auto &slice) {
		return ::conditional_map<Types...>(
			_head, false, lambda(f), slice, 0, slice.size(), evaluation);
	});
}

template<Type... Types, typename F>
inline ExpressionRef Expression::conditional_map(
	const BaseExpressionRef &head,
	const F &f) const {

	return with_slice_c([this, &f] (const auto &slice) {
		return ::conditional_map<Types...>(
			_head, false, lambda(f), slice, 0, slice.size(), false);
	});
}

template<Type... Types, typename F>
inline ExpressionRef Expression::conditional_map(
	const BaseExpressionRef &head,
	const F &f,
	const Evaluation &evaluation) const {

	return with_slice_c([this, &head, &f, &evaluation] (const auto &slice) {
		return ::conditional_map<Types...>(head,
			head.get() != _head.get(), lambda(f), slice, 0, slice.size(), evaluation);
	});
}

template<typename F>
inline ExpressionRef Expression::conditional_map_all(
	const BaseExpressionRef &head,
	const F &f,
	const Evaluation &evaluation) const {

	return with_slice_c([this, &head, &f, &evaluation] (const auto &slice) {
		return ::conditional_map_all(head,
			head.get() != _head.get(), lambda(f), slice, 0, slice.size(), evaluation.parallelize);
	});
}

template<size_t N>
inline const BaseExpressionRef *Expression::n_leaves() const {
    static_assert(N >= 0 && N <= MaxTinySliceSize, "N is too large");
    return static_cast<const TinySlice<N>*>(_slice_ptr)->refs();
}

inline ExpressionRef Expression::slice(
	const BaseExpressionRef &head, index_t begin0, index_t end0) const {

	return with_slice_c([&head, begin0, end0] (const auto &slice) -> ExpressionRef {
		const index_t size = index_t(slice.size());

		index_t begin = begin0;
		index_t end = end0;

		if (begin < 0) {
			begin = size - (-begin % size);
		}
		if (end < 0) {
			end = size - (-end % size);
		}

		end = std::min(end, size);
		begin = std::min(begin, end);

		const size_t new_size = end - begin;
		const SliceCode slice_code = slice.code();

		if (is_packed_slice(slice_code) && new_size >= MinPackedSliceSize) {
			return expression(head, slice.slice(begin, end));
		} else if (slice_code == BigSliceCode && new_size > MaxTinySliceSize) {
			return expression(head, slice.slice(begin, end));
		} else {
			const auto generate_leaves = [begin, end, &head, &slice] (auto &store) {
				for (size_t i = begin; i < end; i++) {
					store(BaseExpressionRef(slice[i]));
				}
			};

			return expression(head, sequential(generate_leaves, new_size));
		}
	});
}

template<typename Slice>
const BaseExpressionRef *ExpressionImplementation<Slice>::materialize(UnsafeBaseExpressionRef &materialized) const {
	auto expr = expression(_head, m_slice.unpack());
	materialized = expr;
	return expr->slice().refs();
}

std::tuple<bool, UnsafeExpressionRef> Expression::thread(const Evaluation &evaluation) const {
	return with_slice([this, &evaluation] (const auto &slice) -> std::tuple<bool, UnsafeExpressionRef> {
		const size_t size = slice.size();

		const auto is_threadable = [] (const BaseExpressionRef &leaf) {
			return (leaf->type() == ExpressionType &&
		        leaf->as_expression()->head()->symbol() == S::List);
		};

		// preflight.

		bool can_thread = false;
		for (size_t i = 0; i < size; i++) {
			if (is_threadable(slice[i])) {
				can_thread = true;
				break;
			}
		}

		if (!can_thread) {
			return std::make_tuple(false, UnsafeExpressionRef(this));
		}

		// actual threading.

		index_t dim = -1;

		LeafVector items;
		std::vector<LeafVector> dim_items;

		for (size_t i = 0; i < size; i++) {
			const BaseExpressionRef &leaf = slice[i];
			if (is_threadable(leaf)) {
				const Expression * const expr = leaf->as_expression();

				if (dim < 0) {
					dim = expr->size();

					expr->with_slice([&items, &dim_items] (const auto &slice) {
						const size_t size = slice.size();
						for (index_t j = 0; j < size; j++) {
							LeafVector element;
							element.reserve(items.size() + 1);
							for (const auto &item : items) {
								element.push_back_copy(item);
							}
							element.push_back_copy(slice[j]);
							dim_items.push_back(std::move(element));
						}
					});

				} else if (dim != expr->size()) {
						// evaluation.message("Thread", "tdlen");
						return std::make_tuple(true, UnsafeExpressionRef(this));
					} else {
						expr->with_slice([&dim_items] (const auto &slice) {
							const size_t size = slice.size();
							for (index_t j = 0; j < size; j++) {
								dim_items[j].push_back_copy(slice[j]);
							}
						});
					}
			} else {
				if (dim < 0) {
					items.push_back_copy(leaf);
				} else {
					for (auto &item : dim_items) {
						item.push_back_copy(leaf);
					}
				}
			}
		}

		if (dim < 0) {
			return std::make_tuple(false, UnsafeExpressionRef(this));
		} else {
			const BaseExpressionRef &head = _head;

			return std::make_tuple(true, expression(
					evaluation.List, sequential([&head, &dim_items] (auto &store) {
				for (auto &items : dim_items) {
					store(expression(head, std::move(items)));
				}
			})));
		}
	});
}

inline ExpressionRef TempVector::to_expression(const BaseExpressionRef &head) const {
    const auto &v = *this;
    return expression(head, sequential([&v] (auto &store) {
        const size_t n = v.size();
        for (size_t i = 0; i < n; i++) {
            store(BaseExpressionRef(v[i]));
        }
    }, size()));
}

inline SortKey Expression::pattern_key() const {
	switch (_head->symbol()) {
		case S::Blank:
			return blank_sort_key(1, size() > 0, this);
		case S::BlankSequence:
			return blank_sort_key(2, size() > 0, this);
		case S::BlankNullSequence:
			return blank_sort_key(3, size() > 0, this);
		case S::PatternTest:
			if (size() != 2) {
				return not_a_pattern_sort_key(this);
			} else {
				SortKey key = n_leaves<2>()[0]->pattern_key();
				key.set_pattern_test(0);
				return key;
			}
		case S::Condition:
			if (size() != 2) {
				return not_a_pattern_sort_key(this);
			} else {
				SortKey key = n_leaves<2>()[0]->pattern_key();
				key.set_condition(0);
				return key;
			}
		case S::Pattern:
			if (size() != 2) {
				return not_a_pattern_sort_key(this);
			} else {
				SortKey key = n_leaves<2>()[1]->pattern_key();
				key.set_pattern_test(0);
				return key;
			}
		case S::Optional:
			if (size() < 1 || size() > 2) {
				return not_a_pattern_sort_key(this);
			} else {
				return with_slice([] (const auto &slice) {
					SortKey key = slice[0]->pattern_key();
					key.set_optional(1);
					return key;
				});
			}
		case S::Alternatives:
			// FIXME
		case S::Verbatim:
			// FIXME
		case S::OptionsPattern:
			// FIXME
		default: {
			return SortKey(2, 0, 1, 1, 0, SortByHead(this, true), SortByLeaves(this, true, true), 1);
		}
	}
}

inline ExpressionRef Expression::flatten_sequence() const {
	return with_slice_c([this] (const auto &slice) -> ExpressionRef {
		if ((slice.type_mask() & TypeMaskSequence) == 0) {
			return ExpressionRef();
		}

		index_t first = -1;

		const size_t n = slice.size();
		for (size_t i = 0; i < n; i++) {
			if (slice[i]->is_sequence()) {
				first = i;
				break;
			}
		}

		if (first >= 0) {
			LeafVector v;

			for (size_t i = 0; i < first; i++) {
				v.push_back_copy(slice[i]);
			}

			for (size_t i = first; i < n; i++) {
				BaseExpressionRef leaf = slice[i];
				if (leaf->is_sequence()) {
					leaf->as_expression()->with_slice([&v] (const auto &sequence) {
						const size_t n = sequence.size();
						for (size_t i = 0; i < n; i++) {
							v.push_back_copy(sequence[i]);
						}
					});
				} else {
					v.push_back(std::move(leaf));
				}
			}

			return expression(head(), std::move(v));
		}

		return ExpressionRef();
	});
}

template<typename F>
inline auto with_slices(const Expression *a, const Expression *b, const F &f) {
	return a->with_slice([b, &f] (const auto &slice_a) {
		return b->with_slice([&f, &slice_a] (const auto &slice_b) {
			return f(slice_a, slice_b);
		});
	});
};

inline bool Expression::same(const BaseExpression &item) const {
	if (this == &item) {
		return true;
	}
	if (!item.is_expression()) {
		return false;
	}

	const Expression * const expr = item.as_expression();

	if (!_head->same(expr->head())) {
		return false;
	}

	if (size() != expr->size()) {
		return false;
	}

	return with_slices(this, item.as_expression(), [] (const auto &a, const auto &b) {
		const size_t n = a.size();
		assert(n == b.size());
		for (size_t i = 0; i < n; i++) {
			if (!a[i]->same(b[i])) {
				return false;
			}
		}
		return true;
	});
}

inline BaseExpressionRef Expression::evaluate_expression_with_non_symbol_head(
	const Evaluation &evaluation) const {

	// Step 4
	// Apply SubValues
	if (_head->is_expression()) {
		const BaseExpression * const head_head =
				_head->as_expression()->_head.get();

		if (head_head->is_symbol()) {
			const Symbol * const head_symbol = head_head->as_symbol();

			const SymbolRules * const rules = head_symbol->state().rules();

			if (rules) {
				const optional<BaseExpressionRef> sub_form =
					rules->sub_rules.apply(this, evaluation);

				if (sub_form) {
					return *sub_form;
				}
			}
		}
	}

	return BaseExpressionRef();
}

inline MatchSize Expression::leaf_match_size() const {
	return with_slice([] (const auto &slice) {
		MatchSize size = MatchSize::exactly(0);
		for (auto leaf : slice) {
			size += leaf->match_size();
		}
		return size;
	});
}

inline ExpressionRef BaseExpression::flatten_sequence() const {
	if (is_expression()) {
		return as_expression()->flatten_sequence();
	} else {
		return ExpressionRef();
	}
}

inline ExpressionRef Expression::flatten_sequence_or_copy() const {
	return coalesce(as_expression()->flatten_sequence(), ExpressionRef(this));
}

inline PatternMatcherRef Expression::expression_matcher() const { // concurrent.
	return ensure_cache()->expression_matcher(this);
}

inline PatternMatcherRef Expression::string_matcher() const { // concurrent.
	return ensure_cache()->string_matcher(this);
}

template<typename T>
inline void intrusive_ptr_add_ref(const T *obj) {
    auto * p = static_cast<const Shared*>(obj);
    p->m_ref_count.fetch_add(1, std::memory_order_relaxed);
};

template<typename T>
inline void intrusive_ptr_release(const T *obj) {
    auto * p = static_cast<const Shared*>(obj);
    if (p->m_ref_count.fetch_add(-1, std::memory_order_relaxed) == 1) {
        const_cast<T*>(obj)->destroy();
    }
};

#endif
