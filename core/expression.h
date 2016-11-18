#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "types.h"
#include "hash.h"
#include "symbol.h"
#include "operations.h"
#include "string.h"
#include "leaves.h"
#include "structure.h"

#include <sstream>
#include <vector>

template<typename T>
class AllOperationsImplementation :
    virtual public OperationsInterface,
    virtual public OperationsImplementation<T>,
    public ArithmeticOperationsImplementation<T>,
	public StructureOperationsImplementation<T> {
public:
    inline AllOperationsImplementation() {
    }
};

template<typename Slice>
class ExpressionImplementation :
	public Expression, public AllOperationsImplementation<ExpressionImplementation<Slice>> {
public:
	virtual ExpressionRef slice(index_t begin, index_t end = INDEX_MAX) const;

public:
	const Slice _leaves;  // other options: ropes, skip lists, ...

	inline ExpressionImplementation(const BaseExpressionRef &head, const Slice &slice) :
        Expression(head, Slice::code(), &_leaves),
        _leaves(slice) {
		assert(head);
	}

	inline ExpressionImplementation(const BaseExpressionRef &head) :
		Expression(head, Slice::code(), &_leaves) {
		assert(head);
	}

	inline ExpressionImplementation(ExpressionImplementation<Slice> &&expr) :
		Expression(expr._head, Slice::code(), &_leaves), _leaves(expr.leaves) {
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

	virtual BaseExpressionRef evaluate_expression_with_non_symbol_head(
		const ExpressionRef &self, const Evaluation &evaluation) const {

		// Step 4
		// Apply SubValues
		if (_head->type() == ExpressionType) {
			auto head_head = boost::static_pointer_cast<const Expression>(_head)->_head.get();
			if (head_head->type() == SymbolType) {
				auto head_symbol = static_cast<const Symbol *>(head_head);

				for (const RuleRef &rule : head_symbol->sub_rules[Slice::code()]) {
					auto child_result = rule->try_apply(self, evaluation);
					if (child_result) {
						return child_result;
					}
				}
			}
		}

		return BaseExpressionRef();
	}

	virtual BaseExpressionRef replace_all(const Match &match) const;

	virtual BaseExpressionRef clone() const;

	virtual BaseExpressionRef clone(const BaseExpressionRef &head) const;

	virtual MatchSize match_size() const {
		const OptionalMatchSize patt_size = _head->match_size_with_head(this);
		if (patt_size) {
			return *patt_size;
		} else {
			MatchSize size = MatchSize::exactly(0);
			for (const auto &leaf : _leaves.leaves()) {
				size += leaf->match_size();
			}
			return size;
		}
	}

	virtual DynamicExpressionRef to_dynamic_expression(const BaseExpressionRef &self) const;

	virtual bool match_leaves(MatchContext &_context, const BaseExpressionRef &patt) const;

	virtual SliceCode slice_code() const {
		return Slice::code();
	}

	virtual const Symbol *lookup_name() const {
		return _head->lookup_name();
	}

	virtual const BaseExpressionRef *materialize(BaseExpressionRef &materialized) const;
};

template<typename U>
inline PackedExpressionRef<U> expression(const BaseExpressionRef &head, const PackedSlice<U> &slice) {
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

inline ExpressionRef expression(
	const BaseExpressionRef &head,
	std::vector<BaseExpressionRef> &&leaves,
	OptionalTypeMask in_type_mask = OptionalTypeMask()) {
	// we expect our callers to move their leaves vector to us. if you cannot move, you
	// should really recheck your design at the site of call.

	if (leaves.size() <= MaxStaticSliceSize) {
		return Heap::StaticExpression(head, leaves);
	} else {
		const TypeMask type_mask = in_type_mask ? *in_type_mask : calc_type_mask(leaves);
		switch (type_mask) {
			case MakeTypeMask(MachineIntegerType):
				return expression(head, PackedSlice<machine_integer_t>(
					collect<MachineInteger, machine_integer_t>(leaves)));
			case MakeTypeMask(MachineRealType):
				return expression(head, PackedSlice<machine_real_t>(
					collect<MachineReal, machine_real_t>(leaves)));
			case MakeTypeMask(StringType):
				return expression(head, PackedSlice<std::string>(
					collect<String, std::string>(leaves)));
			default:
				return Heap::Expression(head, DynamicSlice(leaves, type_mask));
		}
	}
}

inline ExpressionRef expression(
	const BaseExpressionRef &head,
	const std::initializer_list<BaseExpressionRef> &leaves) {

	if (leaves.size() <= MaxStaticSliceSize) {
		return Heap::StaticExpression(head, leaves);
	} else {
		return Heap::Expression(head, DynamicSlice(leaves, OptionalTypeMask()));
	}
}

inline DynamicExpressionRef expression(const BaseExpressionRef &head, const DynamicSlice &slice) {
	return Heap::Expression(head, slice);
}

template<size_t N>
inline StaticExpressionRef<N> expression(const BaseExpressionRef &head, const StaticSlice<N> &slice) {
    return Heap::StaticExpression<N>(head, slice);
}

template<typename Slice>
ExpressionRef ExpressionImplementation<Slice>::slice(index_t begin, index_t end) const {
	const index_t size = index_t(_leaves.size());

	if (begin < 0) {
		begin = size - (-begin % size);
	}
	if (end < 0) {
		end = size - (-end % size);
	}

	end = std::min(end, size);
	begin = std::min(begin, end);

	const size_t new_size = end - begin;
	constexpr SliceCode slice_code = Slice::code();
	const BaseExpressionRef &head = _head;

	if (new_size > MaxStaticSliceSize && (slice_code == DynamicSliceCode || is_packed_slice(slice_code))) {
		return expression(head, _leaves.slice(begin, end));
	} else {
		const Slice &slice = _leaves;

		auto generate_leaves = [begin, end, &head, &slice] (auto &storage) {
			for (size_t i = begin; i < end; i++) {
				storage << slice[i];
			}
		};

		return expression(head, generate_leaves, new_size);
	}
}

template<typename Slice>
DynamicExpressionRef ExpressionImplementation<Slice>::to_dynamic_expression(const BaseExpressionRef &self) const {
	if (std::is_same<Slice, DynamicSlice>()) {
		return boost::static_pointer_cast<DynamicExpression>(self);
	} else {
		std::vector<BaseExpressionRef> leaves;
		leaves.reserve(_leaves.size());
		for (auto leaf : _leaves.leaves()) {
			leaves.push_back(leaf);
		}
		return Heap::Expression(_head, DynamicSlice(std::move(leaves), _leaves.type_mask()));

	}
}

#include "arithmetic_implementation.h"
#include "structure_implementation.h"

#include "evaluate.h"

/*template<typename Slice>
BaseExpressionRef ExpressionImplementation<Slice>::evaluate_from_symbol_head(
	const ExpressionRef &self, const Evaluation &evaluation) const {
	return ::evaluate(self, _head, _leaves, evaluation);
}*/

template<typename Slice>
BaseExpressionRef ExpressionImplementation<Slice>::replace_all(const Match &match) const {
	const BaseExpressionRef &old_head = _head;
	const BaseExpressionRef new_head = old_head->replace_all(match);
	return apply(
		new_head ? new_head : old_head,
		_leaves,
		0,
		_leaves.size(),
		[&match](const BaseExpressionRef &leaf) {
			return leaf->replace_all(match);
		},
		(bool)new_head,
		MakeTypeMask(ExpressionType) | MakeTypeMask(SymbolType));
}

template<typename Slice>
BaseExpressionRef ExpressionImplementation<Slice>::clone() const {
	return expression(_head, _leaves);
}

template<typename Slice>
BaseExpressionRef ExpressionImplementation<Slice>::clone(const BaseExpressionRef &head) const {
	return expression(head, _leaves);
}

template<typename Slice>
const BaseExpressionRef *ExpressionImplementation<Slice>::materialize(BaseExpressionRef &materialized) const {
	auto expr = expression(_head, _leaves.unpack());
	materialized = expr;
	return expr->_leaves.refs();
}

#endif
