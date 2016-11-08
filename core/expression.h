#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "types.h"
#include "hash.h"
#include "symbol.h"
#include "matcher.h"
#include "operations.h"

#include <sstream>
#include <vector>
#include <experimental/optional>

#include "arithmetic.h"

class OperationsInterface :
    virtual public ArithmeticOperations {
};

template<typename T>
class AllOperationsImplementation :
    public OperationsInterface,
    virtual public OperationsImplementation<T>,
    public ArithmeticOperationsImplementation<T> {
public:
    inline AllOperationsImplementation() {
    }

    inline AllOperationsImplementation(const T &expr) : OperationsImplementation<T>(expr) {
    }
};

template<typename Slice>
class ExpressionImplementation : public Expression {
private:
    const AllOperationsImplementation<ExpressionImplementation<Slice>> _operations;

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
				return std::make_shared<ExpressionImplementation<Slice>>(head, _leaves);
			} else {
				return RefsExpressionRef();
			}
		}

		const auto head_symbol = static_cast<const Symbol*>(head.get());
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
				return std::make_shared<ExpressionImplementation<Slice>>(head, _leaves);
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
			auto head_head = std::static_pointer_cast<const Expression>(head)->_head.get();
			if (head_head->type() == SymbolType) {
				auto head_symbol = static_cast<const Symbol*>(head_head);
				// TODO
			}
		}

		// Step 5
		// Evaluate the head with leaves. (DownValue)
		if (head->type() == SymbolType) {
			auto head_symbol = static_cast<const Symbol*>(head.get());

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
        _leaves(leaves),
        _operations(*this) {
		assert(head);
    }

	virtual BaseExpressionRef leaf(size_t i) const {
		return _leaves[i];
	}

	inline BaseExpressionPtr leaf_ptr(size_t i) const {
		return _leaves[i].get();
	}

	virtual size_t size() const {
		return _leaves.size();
	}

	virtual bool same(const BaseExpression &item) const {
		if (this == &item) {
			return true;
		}
		if (item.type() != ExpressionType) {
			return false;
		}
		const Expression *expr = static_cast<const Expression*>(&item);

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
		for (auto leaf : _leaves) {
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
			const auto expr = std::static_pointer_cast<const Expression>(self);
			step3 = evaluate_values(expr, evaluation);
		}

		if (step3) {
			return step3;
		} else {
			return step2;
		}
	}

	virtual bool match_sequence(const Matcher &matcher) const {
		auto patt = matcher.this_pattern();
		auto patt_expr = patt->to_refs_expression(patt);
		return patt_expr->_head->match_sequence_with_head(patt_expr.get(), matcher);
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
		return std::make_shared<ExpressionImplementation<Slice>>(_head, _leaves);
	}

    virtual BaseExpressionRef clone(const BaseExpressionRef &head) const {
        return std::make_shared<ExpressionImplementation<Slice>>(head, _leaves);
    }

    virtual match_sizes_t match_num_args() const {
        return _head->match_num_args_with_head(this);
    }

	virtual RefsExpressionRef to_refs_expression(BaseExpressionRef self) const;

    virtual const OperationsInterface &operations() const {
        return _operations;
    }
};

template<typename U>
using PackExpressionRef = std::shared_ptr<ExpressionImplementation<PackSlice<U>>>;

template<typename U>
inline PackExpressionRef<U> expression(const BaseExpressionRef &head, const PackSlice<U> &slice) {
    return std::make_shared<ExpressionImplementation<PackSlice<U>>>(head, slice);
}

template<typename T>
TypeMask calc_type_mask(const T &container) {
	TypeMask mask = 0;
	for (auto leaf : container) {
		mask |= 1L << leaf->type();
	}
	return mask;
}

template<typename E, typename T>
std::vector<T> collect(const std::vector<BaseExpressionRef> &leaves) {
	std::vector<T> values;
	values.reserve(leaves.size());
	for (auto leaf : leaves) {
		values.push_back(std::static_pointer_cast<const E>(leaf)->value);
	}
	return values;
}

template<typename T>
inline ExpressionRef make_expression(const BaseExpressionRef &head, T leaves) {
	auto type_mask = calc_type_mask(leaves);
	switch (type_mask) {
		case 1L << MachineIntegerType:
			return expression(head, PackSlice<machine_integer_t>(
				collect<MachineInteger, machine_integer_t>(leaves)));
		default:
			return std::make_shared<ExpressionImplementation<RefsSlice>>(
				head, RefsSlice(leaves, type_mask));
	}
}

inline ExpressionRef expression(const BaseExpressionRef &head, std::vector<BaseExpressionRef> &&leaves) {
	return make_expression<std::vector<BaseExpressionRef>>(head, leaves);
}

inline ExpressionRef expression(const BaseExpressionRef &head, const std::vector<BaseExpressionRef> &leaves) {
	return make_expression<const std::vector<BaseExpressionRef>>(head, leaves);
}

inline ExpressionRef expression(const BaseExpressionRef &head, const std::initializer_list<BaseExpressionRef> &leaves) {
	return make_expression<const std::initializer_list<BaseExpressionRef>>(head, leaves);
}

inline ExpressionRef expression(const BaseExpressionRef &head, const RefsSlice &slice) {
	return std::make_shared<ExpressionImplementation<RefsSlice>>(head, slice);
}

template<typename Slice>
template<typename F>
ExpressionRef ExpressionImplementation<Slice>::apply(
	const BaseExpressionRef &head,
	size_t begin,
	size_t end,
	const F &f,
	TypeMask type_mask) const {

	if ((type_mask & _leaves.type_mask()) == 0) {
		return RefsExpressionRef(); // no change
	}

	/*if (_extent.use_count() == 1) {
            // FIXME. optimize this case. we do not need to copy here.
        }*/

	auto slice = _leaves;

	for (size_t i = begin; i < end; i++) {
		auto leaf = slice[i];

		if (((1L << leaf->type()) & type_mask) == 0) {
			continue;
		}

		auto new_leaf = f(leaf);

		if (new_leaf) { // copy is needed now
			const size_t size = slice.size();

			std::vector<BaseExpressionRef> new_leaves;
			new_leaves.reserve(size);

			for (size_t j = 0; j < i; j++) {
				new_leaves.push_back(slice[j]);
			}

			new_leaves.push_back(new_leaf); // leaves[i]

			for (size_t j = i + 1; j < end; j++) {
				auto old_leaf = slice[j];

				if (((1L << old_leaf->type()) & type_mask) == 0) {
					new_leaves.push_back(old_leaf);
				} else {
					new_leaf = f(old_leaf);
					if (new_leaf) {
						new_leaves.push_back(new_leaf);
					} else {
						new_leaves.push_back(old_leaf);
					}
				}
			}

			for (size_t j = end; j < size; j++) {
				new_leaves.push_back(slice[j]);
			}

			return expression(head ? head : _head, std::move(new_leaves));
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
		TypeMask type_mask = 0;
		for (auto leaf : slice) {
			type_mask |= 1L << leaf->type();
			leaves.push_back(leaf);
		}
		return std::make_shared<RefsExpression>(head, RefsSlice(std::move(leaves), type_mask));
	}
};

template<>
class ToRefsExpression<RefsSlice> {
public:
	static inline RefsExpressionRef convert(
		const BaseExpressionRef &expr, const BaseExpressionRef &head, const RefsSlice &slice) {
		return std::static_pointer_cast<const RefsExpression>(expr);
	}
};

template<typename Slice>
RefsExpressionRef ExpressionImplementation<Slice>::to_refs_expression(BaseExpressionRef self) const {
	return ToRefsExpression<Slice>::convert(self, _head, _leaves);
}

#endif
