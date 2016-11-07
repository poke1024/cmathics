#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "types.h"
#include "hash.h"
#include "symbol.h"
#include "matcher.h"
#include <sstream>
#include <vector>
#include <experimental/optional>

template<typename S>
class Expression : public CoreExpression {
public:
	BaseExpressionRef evaluate_head(Evaluation &evaluation) const {
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

		return head;
	}

	S evaluate_leaves(Evaluation &evaluation) const {
		// Evaluate the leaves from left to right (according to Hold values).

		const auto head = _head;

		if (head->type() != SymbolType) {
			return _leaves;
		}

		const auto head_symbol = (Symbol*)head.get();

		// only one type of Hold attribute can be set at a time
		assert(head_symbol->attributes.is_holdfirst +
		       head_symbol->attributes.is_holdrest +
		       head_symbol->attributes.is_holdall +
		       head_symbol->attributes.is_holdallcomplete <= 1);

		size_t eval_leaf_start, eval_leaf_stop;

		if (head_symbol->attributes.is_holdfirst) {
			eval_leaf_start = 1;
			eval_leaf_stop = _leaves.size();
		} else if (head_symbol->attributes.is_holdrest) {
			eval_leaf_start = 0;
			eval_leaf_stop = 1;
		} else if (head_symbol->attributes.is_holdall) {
			// TODO flatten sequences
			eval_leaf_start = 0;
			eval_leaf_stop = 0;
		} else if (head_symbol->attributes.is_holdallcomplete) {
			// no more evaluation is applied
			return _leaves;
		} else {
			eval_leaf_start = 0;
			eval_leaf_stop = _leaves.size();
		}

		assert(0 <= eval_leaf_start);
		assert(eval_leaf_start <= eval_leaf_stop);
		assert(eval_leaf_stop <= _leaves.size());

		return _leaves.apply(
			eval_leaf_start,
			eval_leaf_stop,
			[&evaluation](const BaseExpressionRef &leaf) {
				return leaf->evaluate(leaf, evaluation);
			},
			(1L << ExpressionType) | (1L << SymbolType));
	}

	BaseExpressionRef evaluate_values(CoreExpressionRef self, Evaluation &evaluation) const {
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
	S _leaves;  // other options: ropes, skip lists, ...

    inline Expression(const BaseExpressionRef &head, const S &leaves) :
	    CoreExpression(head), _leaves(leaves) {
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
		const CoreExpression *expr = static_cast<const CoreExpression*>(&item);

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

	virtual BaseExpressionRef evaluate(const BaseExpressionRef &self, Evaluation &evaluation) const {
		// returns empty BaseExpressionRef() if nothing changed

		// Step 1
		const auto head = evaluate_head(evaluation);

		// Step 2
		const auto leaves = evaluate_leaves(evaluation);

		// Step 3, 4, ...
		if (_head == head and _leaves == leaves) {
			const auto expr = std::static_pointer_cast<const Expression>(self);
			return evaluate_values(expr, evaluation);
		} else {
			auto intermediate =  std::make_shared<Expression<S>>(head, leaves);
			return intermediate->evaluate_values(intermediate, evaluation);
		}
	}

	virtual bool match_sequence(const Matcher &matcher) const {
		auto patt = std::static_pointer_cast<const Expression>(matcher.this_pattern());
		return patt->_head->match_sequence_with_head(patt.get(), matcher);
	}

	virtual BaseExpressionRef replace_all(const Match &match) const {
		auto new_head = _head->replace_all(match);

		auto new_leaves = _leaves.apply(
			0,
			_leaves.size(),
			[&match](const BaseExpressionRef &leaf) {
				return leaf->replace_all(match);
			},
			(1L << ExpressionType) | (1L << SymbolType));

		if (new_head || new_leaves != _leaves) {
			if (!new_head) {
				new_head = _head;
			}
			return std::make_shared<Expression<S>>(new_head, new_leaves);
		} else {
			return BaseExpressionRef();
		}
	}

	virtual BaseExpressionRef clone() const {
		return std::make_shared<Expression<S>>(_head, _leaves);
	}

    virtual match_sizes_t match_num_args() const {
        return _head->match_num_args_with_head(this);
    }

	virtual RefsExpressionRef to_refs_expression(BaseExpressionRef self) const {
		return std::static_pointer_cast<const RefsExpression>(self);
	}
};

inline RefsExpressionRef expression(const BaseExpressionRef &head, const RefsSlice &slice) {
    return std::make_shared<Expression<RefsSlice>>(head, slice);
}

#endif
