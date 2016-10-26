#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "types.h"
#include "hash.h"
#include <vector>

class Expression : public BaseExpression {
private:
    BaseExpressionRef evaluate2(const BaseExpressionRef &head, const Slice &leaves) const;
    BaseExpressionRef evaluate3() const;

public:
    mutable hash_t _computed_hash;

    BaseExpressionRef _head;
    Slice _leaves;  // other options: ropes, skip lists, ...

    inline Expression(BaseExpressionRef head, const Slice &leaves) :
        _head(head),
        _leaves(leaves),
        _computed_hash(0) {
    }

    inline Expression(BaseExpressionRef head, std::initializer_list<BaseExpressionRef> leaves) :
        _head(head),
        _leaves(leaves.begin(), leaves.size()),
        _computed_hash(0) {
    }

    inline Expression(BaseExpressionRef head, const std::vector<BaseExpressionRef> &leaves) :
        _head(head),
        _leaves(&leaves[0], leaves.size()),
        _computed_hash(0) {
    }

    inline const BaseExpressionRef &leaf(size_t i) const {
        return _leaves.leaf(i);
    }

    inline BaseExpressionPtr leaf_ptr(size_t i) const {
        return _leaves.leaf_ptr(i);
    }

    virtual Type type() const {
        return ExpressionType;
    }

    virtual bool same(BaseExpressionPtr item) const;

    virtual hash_t hash() const;

    virtual std::string fullform() const;

    virtual BaseExpressionRef evaluate() const;

    virtual BaseExpressionRef get_head() const {
        return _head;
    }

    virtual BaseExpressionPtr get_head_ptr() const {
        return _head.get();
    }

    virtual bool is_sequence() const {
        return _head->is_symbol_sequence();
    }

    virtual match_sizes_t match_num_args() const {
        return _head->match_num_args_with_head(this);
    }

    virtual Match match_sequence(
        const Matcher &matcher) const;
};

inline Slice get_sequence(const BaseExpressionRef &expr) {
    if (expr->is_sequence()) {
        return Slice(std::static_pointer_cast<const Expression>(expr)->_leaves);
    } else {
        return Slice(expr);
    }
}
#endif
