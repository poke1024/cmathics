#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <assert.h>

#include "expression.h"
#include "hash.h"
#include "definitions.h"
#include "pattern.h"

bool Expression::same(BaseExpressionPtr item) const {
    if (this == item) {
        return true;
    }
    if (item->type() != ExpressionType) {
        return false;
    }
    const Expression *expr = static_cast<const Expression*>(item);

    if (!_head->same(expr->_head.get())) {
        return false;
    }

    const size_t n_leaves = _leaves.size();
    if (n_leaves != expr->_leaves.size()) {
        return false;
    }
    for (size_t i = 0; i < n_leaves; i++) {
        if (!_leaves[i]->same(expr->_leaves[i].get())) {
            return false;
        }
    }

    return true;
}

hash_t Expression::hash() const {
    hash_t hash = _computed_hash;
    if (hash) {
        return hash;
    }
    hash_t result = hash_combine(result, _head->hash());
    for (auto leaf : _leaves) {
        result = hash_combine(result, leaf->hash());
    }
    if (result == 0) {
        result = 1;  // enforce non-zero hash
    }
    _computed_hash = result;
    return result;
}

std::string Expression::fullform() const {
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

// returns nullptr if nothing changed
BaseExpressionRef Expression::evaluate() const {
    // Step 1
    // Evaluate the head
    auto head = _head;

    while (true) {
        auto child_result = head->evaluate();
        if (child_result == nullptr)
            break;
        head = child_result;
    }

    // Step 2
    // Evaluate the leaves from left to right (according to Hold values).

    size_t eval_leaf_start, eval_leaf_stop;

    if (head->type() == SymbolType) {
        auto head_symbol = (Symbol*)head.get();

        // only one type of Hold attribute can be set at a time
        assert(head_symbol->attributes.is_holdfirst +
               head_symbol->attributes.is_holdrest +
               head_symbol->attributes.is_holdall +
               head_symbol->attributes.is_holdallcomplete <= 1);

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
            return NULL;
        } else {
            eval_leaf_start = 0;
            eval_leaf_stop = _leaves.size();
        }

        assert(0 <= eval_leaf_start);
        assert(eval_leaf_start <= eval_leaf_stop);
        assert(eval_leaf_stop <= _leaves.size());

        // perform the evaluation
        return evaluate2(head, _leaves.apply(eval_leaf_start, eval_leaf_stop, [](const BaseExpressionRef &leaf) mutable {
            return leaf->evaluate();
        }));
    } else {
        return evaluate2(head, _leaves);
    }
}

BaseExpressionRef Expression::evaluate2(const BaseExpressionRef &head, const Slice &leaves) const {
    if (_head == head and _leaves == leaves) {
        return evaluate3();
    } else {
        return Expression(head, leaves).evaluate3();
    }
}

BaseExpressionRef Expression::evaluate3() const {
    auto head = _head;

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

        // first try to apply CFunction (if present)
        if (head_symbol->down_code != nullptr) {
            auto child_result = (head_symbol->down_code)(
                std::static_pointer_cast<const Expression>(shared_from_this()));
            if (child_result != nullptr) {
                return child_result;
            }
        }

        // if the CFunction is absent or returns NULL then apply downvalues
        for (size_t i = 0; i < head_symbol->down_values->_leaves.size(); i++) {
            auto child_result = replace(head_symbol->down_values->_leaves[i]);
            if (child_result != nullptr) {
                return child_result;
            }
        }
    }

    return BaseExpressionRef();
}

Match Expression::match_sequence(const Matcher &matcher) const {
    auto patt = std::static_pointer_cast<const Expression>(matcher.this_pattern());
    return patt->_head->match_sequence_with_head(patt, matcher);
}
