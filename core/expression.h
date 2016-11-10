#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "types.h"
#include "hash.h"
#include "symbol.h"
#include "operations.h"
#include "string.h"
#include "leaves.h"

#include <sstream>
#include <vector>

template<typename T>
class AllOperationsImplementation :
    virtual public OperationsInterface,
    virtual public OperationsImplementation<T>,
    public ArithmeticOperationsImplementation<T> {
public:
    inline AllOperationsImplementation() {
    }
};

template<typename Slice>
class ExpressionImplementation :
	public Expression, public AllOperationsImplementation<ExpressionImplementation<Slice>> {
public:
	template<typename F>
	ExpressionRef apply(
		const BaseExpressionRef &head,
		size_t begin,
		size_t end,
		const F &f,
		TypeMask type_mask) const;

	virtual ExpressionRef slice(index_t begin, index_t end = INDEX_MAX) const;

	ExpressionRef evaluate_head_and_leaves(const Evaluation &evaluation) const {
		// Evaluate the head

		auto head = _head;

		while (true) {
			auto new_head = head->evaluate(head, evaluation);
			if (new_head) {
				head = new_head;
			} else {
				break;
			}
		}

		// Evaluate the leaves from left to right (according to Hold values).

		if (head->type() != SymbolType) {
			if (head != _head) {
				return Heap::Expression(head, _leaves);
			} else {
				return RefsExpressionRef();
			}
		}

		const auto head_symbol = static_cast<const Symbol *>(head.get());
		const auto attributes = head_symbol->attributes;

		// only one type of Hold attribute can be set at a time
		assert(count(attributes,
		             Attributes::HoldFirst +
		             Attributes::HoldRest +
		             Attributes::HoldAll +
		             Attributes::HoldAllComplete) <= 1);

		size_t eval_leaf_start, eval_leaf_stop;

		if (attributes & Attributes::HoldFirst) {
			eval_leaf_start = 1;
			eval_leaf_stop = _leaves.size();
		} else if (attributes & Attributes::HoldRest) {
			eval_leaf_start = 0;
			eval_leaf_stop = 1;
		} else if (attributes & Attributes::HoldAll) {
			// TODO flatten sequences
			eval_leaf_start = 0;
			eval_leaf_stop = 0;
		} else if (attributes & Attributes::HoldAllComplete) {
			// no more evaluation is applied
			if (head != _head) {
				return Heap::Expression(head, _leaves);
			} else {
				return RefsExpressionRef();
			}
		} else {
			eval_leaf_start = 0;
			eval_leaf_stop = _leaves.size();
		}

		assert(0 <= eval_leaf_start);
		assert(eval_leaf_start <= eval_leaf_stop);
		assert(eval_leaf_stop <= _leaves.size());

		return apply(
			head,
			eval_leaf_start,
			eval_leaf_stop,
			[&evaluation](const BaseExpressionRef &leaf) {
				return leaf->evaluate(leaf, evaluation);
			},
			(1L << ExpressionType) | (1L << SymbolType));
	}

	virtual BaseExpressionRef evaluate_values(const ExpressionRef &self, const Evaluation &evaluation) const {
		const auto head = _head;

		// Step 3
		// Apply UpValues for leaves
		// TODO

		// Step 4
		// Apply SubValues
		if (head->type() == ExpressionType) {
			auto head_head = boost::static_pointer_cast<const Expression>(head)->_head.get();
			if (head_head->type() == SymbolType) {
				auto head_symbol = static_cast<const Symbol *>(head_head);
				// TODO
			}
		}

		// Step 5
		// Evaluate the head with leaves. (DownValue)
		if (head->type() == SymbolType) {
			auto head_symbol = static_cast<const Symbol *>(head.get());

			for (const Rule &rule : head_symbol->down_rules) {
				auto child_result = rule(self, evaluation);
				if (child_result) {
					return child_result;
				}
			}
		}

		return BaseExpressionRef();
	}

public:
	const Slice _leaves;  // other options: ropes, skip lists, ...

	inline ExpressionImplementation(const BaseExpressionRef &head, const Slice &leaves) :
        Expression(head),
        _leaves(leaves) {
		assert(head);
	}

	inline ExpressionImplementation(ExpressionImplementation<Slice> &&expr) :
		Expression(expr._head), _leaves(expr.leaves) {
	}

	virtual BaseExpressionRef leaf(size_t i) const {
		return _leaves[i];
	}

	inline auto leaves() const {
		return _leaves.leaves();
	}

	template<typename T>
	inline auto primitives() const {
		return _leaves.template primitives<T>();
	}

	inline auto begin() const {
		return _leaves.begin();
	}

	inline auto end() const {
		return _leaves.begin();
	}

	virtual size_t size() const {
		return _leaves.size();
	}

	virtual TypeMask type_mask() const {
		return _leaves.type_mask();
	}

	virtual bool same(const BaseExpression &item) const {
		if (this == &item) {
			return true;
		}
		if (item.type() != ExpressionType) {
			return false;
		}
		const Expression *expr = static_cast<const Expression *>(&item);

		if (!_head->same(expr->head())) {
			return false;
		}

		const size_t size = _leaves.size();
		if (size != expr->size()) {
			return false;
		}
		for (size_t i = 0; i < size; i++) {
			if (!_leaves[i]->same(expr->leaf(i))) {
				return false;
			}
		}

		return true;
	}

	virtual hash_t hash() const {
		hash_t result = hash_combine(result, _head->hash());
		for (auto leaf : _leaves.leaves()) {
			result = hash_combine(result, leaf->hash());
		}
		return result;
	}

	virtual std::string fullform() const {
		std::stringstream result;

		// format head
		result << _head->fullform();
		result << "[";

		// format leaves
		const size_t argc = _leaves.size();

		for (size_t i = 0; i < argc; i++) {
			result << _leaves[i]->fullform();

			if (i < argc - 1) {
				result << ", ";
			}
		}
		result << "]";

		return result.str();
	}

	virtual BaseExpressionRef evaluate(const BaseExpressionRef &self, const Evaluation &evaluation) const {
		BaseExpressionRef expr = self;
		while (true) {
			BaseExpressionRef evaluated = expr->evaluate_once(expr, evaluation);
			if (evaluated) {
				expr = evaluated;
			} else {
				break;
			}
		}
		return expr == self ? BaseExpressionRef() : expr;
	}

	virtual BaseExpressionRef evaluate_once(const BaseExpressionRef &self, const Evaluation &evaluation) const {
		// returns empty BaseExpressionRef() if nothing changed

		// Step 1, 2
		const auto step2 = evaluate_head_and_leaves(evaluation);

		// Step 3, 4, ...
		BaseExpressionRef step3;
		if (step2) {
			step3 = step2->evaluate_values(step2, evaluation);
		} else {
			const auto expr = boost::static_pointer_cast<const Expression>(self);
			step3 = evaluate_values(expr, evaluation);
		}

		if (step3) {
			return step3;
		} else {
			return step2;
		}
	}

	virtual BaseExpressionRef replace_all(const Match &match) const {
		return apply(
			_head->replace_all(match),
            0,
            _leaves.size(),
            [&match](const BaseExpressionRef &leaf) {
                return leaf->replace_all(match);
            },
            (1L << ExpressionType) | (1L << SymbolType));
}

	virtual BaseExpressionRef clone() const {
		return Heap::Expression(_head, _leaves);
	}

	virtual BaseExpressionRef clone(const BaseExpressionRef &head) const {
		return Heap::Expression(head, _leaves);
	}

	virtual match_sizes_t match_num_args() const {
		return _head->match_num_args_with_head(this);
	}

	virtual RefsExpressionRef to_refs_expression(const BaseExpressionRef &self) const;

	virtual bool match_leaves(MatchContext &_context, const BaseExpressionRef &patt) const;

	virtual SliceTypeId slice_type_id() const {
		return _leaves.type_id();
	}
};

template<typename U>
inline PackExpressionRef<U> expression(const BaseExpressionRef &head, const PackSlice<U> &slice) {
	return Heap::Expression(head, slice);
}

template<typename E, typename T>
std::vector<T> collect(const std::vector<BaseExpressionRef> &leaves) {
	std::vector<T> values;
	values.reserve(leaves.size());
	for (auto leaf : leaves) {
		values.push_back(boost::static_pointer_cast<const E>(leaf)->value);
	}
	return values;
}

template<typename T>
inline ExpressionRef make_expression(const BaseExpressionRef &head, const T &leaves) {
	const auto size = leaves.size();
    switch (size) {
        case 0: // e.g. {}
            return Heap::Expression(head, InPlaceRefsSlice<0>());
        case 1:
            return Heap::Expression(head, InPlaceRefsSlice<1>(leaves, OptionalTypeMask()));
        case 2:
            return Heap::Expression(head, InPlaceRefsSlice<2>(leaves, OptionalTypeMask()));
        case 3:
            return Heap::Expression(head, InPlaceRefsSlice<3>(leaves, OptionalTypeMask()));
    }
	const auto type_mask = calc_type_mask(leaves);
	switch (type_mask) {
		case 1L << MachineIntegerType:
			return expression(head, PackSlice<machine_integer_t>(
				collect<MachineInteger, machine_integer_t>(leaves)));
		case 1L << MachineRealType:
			return expression(head, PackSlice<machine_real_t>(
				collect<MachineReal, machine_real_t>(leaves)));
		case 1L << StringType:
			return expression(head, PackSlice<std::string>(
				collect<String, std::string>(leaves)));
		default:
			return Heap::Expression(head, RefsSlice(leaves, type_mask));
	}
}

inline ExpressionRef expression(const BaseExpressionRef &head, const std::vector<BaseExpressionRef> &&leaves) {
	return make_expression<std::vector<BaseExpressionRef>>(head, std::move(leaves)); // FIXME
}

inline ExpressionRef expression(const BaseExpressionRef &head, const std::vector<BaseExpressionRef> &leaves) {
	return make_expression<std::vector<BaseExpressionRef>>(head, leaves);
}

inline ExpressionRef expression(const BaseExpressionRef &head, const std::initializer_list<BaseExpressionRef> &leaves) {
	return make_expression<std::initializer_list<BaseExpressionRef>>(head, leaves);
}

inline ExpressionRef expression(const BaseExpressionRef &head, const RefsSlice &slice) {
	return Heap::Expression(head, slice);
}

template<size_t N>
inline ExpressionRef expression(const BaseExpressionRef &head, const InPlaceRefsSlice<N> &slice) {
    return Heap::Expression(head, slice);
}

class heap_storage {
private:
	std::vector<BaseExpressionRef> leaves;

public:
	inline heap_storage(size_t size) {
		leaves.reserve(size);
	}

	inline heap_storage &operator<<(const BaseExpressionRef &expr) {
		leaves.push_back(expr);
		return *this;
	}

	inline ExpressionRef to_expression(const BaseExpressionRef &head) {
		return expression(head, std::move(leaves));
	}
};

template<size_t N>
class stack_storage_base {
protected:
	BaseExpressionRef _leaves[N];
	size_t _i = 0;

public:
	inline stack_storage_base &operator<<(const BaseExpressionRef &expr) {
		_leaves[_i++] = expr;
		return *this;
	}
};

template<size_t N>
class stack_storage {
};


template<>
class stack_storage<1> : public stack_storage_base<1> {
public:
	inline ExpressionRef to_expression(const BaseExpressionRef &head) {
		const auto &l = _leaves;
		return expression(head, {l[0]});
	}
};

template<>
class stack_storage<2> : public stack_storage_base<2> {
public:
	inline ExpressionRef to_expression(const BaseExpressionRef &head) {
		const auto &l = _leaves;
		return expression(head, {l[0], l[1]});
	}
};

template<>
class stack_storage<3> : public stack_storage_base<3> {
public:
	inline ExpressionRef to_expression(const BaseExpressionRef &head) {
		const auto &l = _leaves;
		return expression(head, {l[0], l[1], l[2]});
	}
};

template<typename Slice, typename F, typename Array>
void apply(size_t i0, const BaseExpressionRef &leaf0, size_t end, size_t size, const Slice &slice,
	const F &f, TypeMask type_mask, Array &array) {
	for (size_t j = 0; j < i0; j++) {
		array << slice[j];
	}

	array << leaf0; // leaves[i0]

	for (size_t j = i0 + 1; j < end; j++) {
		const auto &old_leaf = slice[j];

		if ((old_leaf->type_mask() & type_mask) == 0) {
			array << old_leaf;
		} else {
			auto new_leaf = f(old_leaf);
			if (new_leaf) {
				array << new_leaf;
			} else {
				array << old_leaf;
			}
		}
	}

	for (size_t j = end; j < size; j++) {
		array << slice[j];
	}
}

template<typename Slice, size_t N, typename F>
inline ExpressionRef apply_tiny(const BaseExpressionRef &head, size_t i0, const BaseExpressionRef &new_leaf,
	size_t end, const Slice &slice, const F &f, TypeMask type_mask) {
	assert(slice.size() == N);
	stack_storage<N> array;
	apply(i0, new_leaf, end, N, slice, f, type_mask, array);
	return array.to_expression(head);
}

template<typename Slice, typename F>
inline ExpressionRef apply_regular(const BaseExpressionRef &head, size_t i0, const BaseExpressionRef &new_leaf,
   size_t end, const Slice &slice, const F &f, TypeMask type_mask) {
	const size_t size = slice.size();
	heap_storage array(size);
	apply(i0, new_leaf, end, size, slice, f, type_mask, array);
	return array.to_expression(head);
}

template<typename Slice>
template<typename F>
ExpressionRef ExpressionImplementation<Slice>::apply(
	const BaseExpressionRef &head,
	size_t begin,
	size_t end,
	const F &f,
	TypeMask type_mask) const {

	const auto &slice = _leaves;

	if ((type_mask & _leaves.type_mask()) != 0) {
		/*if (_extent.use_count() == 1) {
			// FIXME. optimize this case. we do not need to copy here.
		}*/

		for (size_t i = begin; i < end; i++) {
			const auto &leaf = slice[i];

			if ((leaf->type_mask() & type_mask) == 0) {
				continue;
			}

			const auto new_leaf = f(leaf);

			if (new_leaf) { // copy is needed now
				const BaseExpressionRef &new_head = head ? head : _head;

				switch (slice.size()) {
					case 1:
						return apply_tiny<Slice, 1, F>(new_head, i, new_leaf, end, slice, f, type_mask);
					case 2:
						return apply_tiny<Slice, 2, F>(new_head, i, new_leaf, end, slice, f, type_mask);
					case 3:
						return apply_tiny<Slice, 3, F>(new_head, i, new_leaf, end, slice, f, type_mask);
					default:
						return apply_regular<Slice, F>(new_head, i, new_leaf, end, slice, f, type_mask);
				}
			}
		}
	}

	if (head == _head) {
		return RefsExpressionRef(); // no change
	} else {
		return expression(head ? head : _head, slice);
	}
}

template<typename Slice>
ExpressionRef ExpressionImplementation<Slice>::slice(index_t begin, index_t end) const {
	return expression(_head, _leaves.slice(begin, end));
}

template<typename Slice>
class ToRefsExpression {
public:
	static inline RefsExpressionRef convert(
		const BaseExpressionRef &expr, const BaseExpressionRef &head, const Slice &slice) {
		std::vector<BaseExpressionRef> leaves;
		leaves.reserve(slice.size());
		for (auto leaf : slice.leaves()) {
			leaves.push_back(leaf);
		}
		return Heap::Expression(head, RefsSlice(std::move(leaves), slice.type_mask()));
	}
};

template<>
class ToRefsExpression<RefsSlice> {
public:
	static inline RefsExpressionRef convert(
		const BaseExpressionRef &expr, const BaseExpressionRef &head, const RefsSlice &slice) {
		return boost::static_pointer_cast<const RefsExpression>(expr);
	}
};

template<typename Slice>
RefsExpressionRef ExpressionImplementation<Slice>::to_refs_expression(const BaseExpressionRef &self) const {
	return ToRefsExpression<Slice>::convert(self, _head, _leaves);
}

#include "arithmetic_implementation.h"

#endif
