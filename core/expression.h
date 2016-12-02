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

#include <symengine/add.h>
#include <symengine/mul.h>
#include <symengine/pow.h>

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
protected:
	virtual BaseExpressionRef slow_leaf(size_t i) const {
		return _leaves[i];
	}

public:
	virtual ExpressionRef slice(index_t begin, index_t end = INDEX_MAX) const;

public:
	const Slice _leaves;  // other options: ropes, skip lists, ...

	inline ExpressionImplementation(const BaseExpressionRef &head, const Slice &slice) :
        Expression(head, Slice::code(), &_leaves), _leaves(slice) {
		assert(head);
	}

	inline ExpressionImplementation(const BaseExpressionRef &head) :
		Expression(head, Slice::code(), &_leaves), _leaves() {
		assert(head);
	}

	inline ExpressionImplementation(ExpressionImplementation<Slice> &&expr) :
		Expression(expr._head, Slice::code(), &_leaves), _leaves(expr.leaves) {
	}

	inline auto leaves() const {
		return _leaves.leaves();
	}

	template<typename T>
	inline auto primitives() const {
		return _leaves.template primitives<T>();
	}

	inline TypeMask type_mask() const {
		return _leaves.type_mask();
	}

	inline TypeMask exact_type_mask() const {
		return _leaves.exact_type_mask();
	}

	inline void init_type_mask(TypeMask type_mask) const {
		_leaves.init_type_mask(type_mask);
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
		const Evaluation &evaluation) const {

		// Step 4
		// Apply SubValues
		if (_head->type() == ExpressionType) {
			const BaseExpression * const head_head =
				static_cast<const Expression*>(_head.get())->_head.get();

			if (head_head->type() == SymbolType) {
				const Symbol * const head_symbol = static_cast<const Symbol *>(head_head);

				const SymbolRules * const rules = head_symbol->rules();

				if (rules) {
					const BaseExpressionRef sub_form =
						rules->sub_rules.try_and_apply<Slice>(this, evaluation);

					if (sub_form) {
						return sub_form;
					}
				}
			}
		}

		return BaseExpressionRef();
	}

	virtual BaseExpressionRef replace_all(const Match &match) const;

	virtual BaseExpressionRef clone() const;

	virtual BaseExpressionRef clone(const BaseExpressionRef &head) const;

	virtual hash_t hash() const {
		hash_t result = hash_combine(_leaves.size(), _head->hash());
		for (const auto &leaf : _leaves.leaves()) {
			result = hash_combine(result, leaf->hash());
		}
		return result;
	}

	virtual optional<hash_t> compute_match_hash() const {
		switch (_head->extended_type()) {
			case SymbolBlank:
			case SymbolBlankSequence:
			case SymbolBlankNullSequence:
			case SymbolPattern:
			case SymbolAlternatives:
			case SymbolRepeated:
				return optional<hash_t>();

			default: {
				// note that this must yield the same value as hash()
				// defined above if this Expression is not a pattern.

				const auto head_hash = _head->match_hash();
				if (!head_hash) {
					return optional<hash_t>();
				}

				hash_t result = hash_combine(_leaves.size(), *head_hash);
				for (const auto &leaf : _leaves.leaves()) {
					const auto leaf_hash = leaf->match_hash();
					if (!leaf_hash) {
						return optional<hash_t>();
					}
					result = hash_combine(result, *leaf_hash);
				}
				return result;
			}
		}
	}

	virtual MatchSize match_size() const {
		switch (_head->extended_type()) {
			case SymbolBlank:
				return MatchSize::exactly(1);
			case SymbolBlankSequence:
				return MatchSize::at_least(1);
			case SymbolBlankNullSequence:
				return MatchSize::at_least(0);
			case SymbolPattern:
				if (size() == 2) {
					// Pattern is only valid with two arguments
					return _leaves[1]->match_size();
				} else {
					return MatchSize::exactly(1);
				}
			case SymbolAlternatives: {
				const size_t n = size();

				if (n == 0) {
					return MatchSize::exactly(1);
				}

				const MatchSize size = _leaves[0]->match_size();
				match_size_t min_p = size.min();
				match_size_t max_p = size.max();

				for (size_t i = 1; i < n; i++) {
					const MatchSize leaf_size = _leaves[i]->match_size();
					max_p = std::max(max_p, leaf_size.max());
					min_p = std::min(min_p, leaf_size.min());
				}

				return MatchSize::between(min_p, max_p);
			}
			case SymbolRepeated:
				switch(size()) {
					case 1:
						return MatchSize::at_least(1);
					case 2:
						// TODO inspect second arg
						return MatchSize::at_least(1);
					default:
						return MatchSize::exactly(1);
				}

			default: {
				MatchSize size = MatchSize::exactly(0);
				for (const auto &leaf : _leaves.leaves()) {
					size += leaf->match_size();
				}
				return size;
			}
		}
	}

	virtual DynamicExpressionRef to_dynamic_expression(const BaseExpressionRef &self) const;

	virtual bool match_leaves(MatchContext &_context, const BaseExpressionRef &patt) const;

	virtual const Symbol *lookup_name() const {
		return _head->lookup_name();
	}

	virtual const BaseExpressionRef *materialize(BaseExpressionRef &materialized) const;

	virtual SortKey pattern_key() const {
		switch (_head->extended_type()) {
			case SymbolBlank:
				return SortKey::Blank(1, size() > 0, this);
			case SymbolBlankSequence:
				return SortKey::Blank(2, size() > 0, this);
			case SymbolBlankNullSequence:
				return SortKey::Blank(3, size() > 0, this);
			case SymbolPatternTest:
				if (size() != 2) {
					return SortKey::NotAPattern(this);
				} else {
					SortKey key = _leaves[0]->pattern_key();
					key.pattern_test = 0;
					return key;
				}
			case SymbolCondition:
				if (size() != 2) {
					return SortKey::NotAPattern(this);
				} else {
					SortKey key = _leaves[0]->pattern_key();
					key.condition = 0;
					return key;
				}
			case SymbolPattern:
				if (size() != 2) {
					return SortKey::NotAPattern(this);
				} else {
					SortKey key = _leaves[1]->pattern_key();
					key.pattern = 0;
					return key;
				}
			case SymbolOptional:
				if (size() < 1 || size() > 2) {
					return SortKey::NotAPattern(this);
				} else {
					SortKey key = _leaves[0]->pattern_key();
					key.optional = 1;
					return key;
				}
			case SymbolAlternatives:
				// FIXME
			case SymbolVerbatim:
				// FIXME
			case SymbolOptionsPattern:
				// FIXME
			default: {
				SortKey key(2, 0, 1, 1, 0, this, 1);
				key.leaf_precedence = true;
				return key;
			}
		}
	}

    template<typename Compute, typename Recurse>
    BaseExpressionRef do_symbolic(
        const Compute &compute,
        const Recurse &recurse,
        const Evaluation &evaluation) const {

        const SymbolicForm form = symbolic_form();

        if (!form.is_null()) {
            const optional<SymbolicForm> new_form = compute(form);
            if (new_form) {
                return from_symbolic_form(*new_form, evaluation);
            } else {
                return BaseExpressionRef();
            }
        } else {
            return apply(
                _head,
                _leaves,
                0,
                _leaves.size(),
                [&recurse, &evaluation] (const BaseExpressionRef &leaf) {
                    return recurse(leaf, evaluation);
                },
                false,
                MakeTypeMask(ExpressionType));
        }
    }

	virtual BaseExpressionRef expand(const Evaluation &evaluation) const {
        return do_symbolic(
            [] (const SymbolicForm &form) {
                const SymbolicForm new_form = SymEngine::expand(form);
                if (new_form.get() != form.get()) {
                    return optional<SymbolicForm>(new_form);
                } else {
                    return optional<SymbolicForm>();
                }
            },
            [] (const BaseExpressionRef &leaf, const Evaluation &evaluation) {
                return leaf->expand(evaluation);
            },
            evaluation);
	}

	virtual optional<SymEngine::vec_basic> symbolic_operands() const {
		for (const auto &leaf : _leaves.leaves()) {
			if (leaf->no_symbolic_form()) {
				return optional<SymEngine::vec_basic>();
			}
		}

		SymEngine::vec_basic operands;
        operands.reserve(size());
		for (const auto &leaf : _leaves.leaves()) {
            const SymbolicForm form = leaf->symbolic_form();
            if (form.is_null()) {
                return optional<SymEngine::vec_basic>();
            }
			operands.push_back(form);
		}

		return operands;
	}
};

#include "leaves.tcc"

template<size_t N>
inline const BaseExpressionRef *Expression::static_leaves() const {
    static_assert(N <= MaxStaticSliceSize, "N is too large");
    return static_cast<const StaticSlice<N>*>(_slice_ptr)->refs();
}

inline bool Expression::symbolic_1(const SymEngineUnaryFunction &f) const {
    const SymbolicForm symbolic_a = static_leaves<1>()[0]->symbolic_form();
    if (!symbolic_a.is_null()) {
        _symbolic_form = f(symbolic_a);
        return true;
    } else {
        return false;
    }
}

inline bool Expression::symbolic_2(const SymEngineBinaryFunction &f) const {
	const BaseExpressionRef * const leaves = static_leaves<2>();
	const BaseExpressionRef &a = leaves[0];
	const BaseExpressionRef &b = leaves[1];

	if (a->no_symbolic_form() || b->no_symbolic_form()) {
		return false;
	}

    const SymbolicForm symbolic_a = a->symbolic_form();
    if (!symbolic_a.is_null()) {
        const SymbolicForm symbolic_b = b->symbolic_form();
        if (!symbolic_b.is_null()) {
            _symbolic_form = f(symbolic_a, symbolic_b);
            return true;
        }
    }
    return false;
}

template<typename U>
inline PackedExpressionRef<U> expression(const BaseExpressionRef &head, const PackedSlice<U> &slice) {
	return Heap::Expression(head, slice);
}

template<typename E, typename T>
std::vector<T> collect(const std::vector<BaseExpressionRef> &leaves) {
	std::vector<T> values;
	values.reserve(leaves.size());
	for (const auto &leaf : leaves) {
		values.push_back(static_cast<const E*>(leaf.get())->value);
	}
	return values;
}

inline ExpressionRef expression(
	const BaseExpressionRef &head,
	std::vector<BaseExpressionRef> &&leaves,
	TypeMask some_type_mask = UnknownTypeMask) {
	// we expect our callers to move their leaves vector to us. if you cannot move, you
	// should really recheck your design at the site of call.

	const size_t size = leaves.size();

	if (size <= MaxStaticSliceSize) {
		return Heap::StaticExpression(head, leaves);
	} else if (size < MinPackedSliceSize) {
		return Heap::Expression(head, DynamicSlice(leaves, some_type_mask));
	} else {
		const TypeMask type_mask = is_exact_type_mask(some_type_mask) ?
            some_type_mask : exact_type_mask(leaves);
		switch (type_mask) {
			case MakeTypeMask(MachineIntegerType):
				return expression(head, PackedSlice<machine_integer_t>(
					collect<MachineInteger, machine_integer_t>(leaves)));
			case MakeTypeMask(MachineRealType):
				return expression(head, PackedSlice<machine_real_t>(
					collect<MachineReal, machine_real_t>(leaves)));
			default:
				return Heap::Expression(head, DynamicSlice(leaves, type_mask));
		}
	}
}

inline ExpressionRef expression(
	const BaseExpressionRef &head) {

	return Heap::EmptyExpression(head);
}

inline ExpressionRef expression(
    const BaseExpressionRef &head,
    const BaseExpressionRef &a) {

    return Heap::StaticExpression<1>(head, StaticSlice<1>({a}));
}

inline ExpressionRef expression(
    const BaseExpressionRef &head,
    const BaseExpressionRef &a,
    const BaseExpressionRef &b) {

    return Heap::StaticExpression<2>(head, StaticSlice<2>({a, b}));
}

inline ExpressionRef expression(
	const BaseExpressionRef &head,
	const std::initializer_list<BaseExpressionRef> &leaves) {

	if (leaves.size() <= MaxStaticSliceSize) {
		return Heap::StaticExpression(head, leaves);
	} else {
		return Heap::Expression(head, DynamicSlice(leaves, UnknownTypeMask));
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

#include "definitions.h"
#include "evaluation.h"

#include "arithmetic_implementation.h"
#include "structure_implementation.h"

#include "evaluate.h"

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
