#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "types.h"
#include "hash.h"
#include <valarray>
#include <vector>

class Expression : public BaseExpression {
private:
    BaseExpressionPtr evaluate2(BaseExpressionPtr head, const Slice &leaves) const;
    BaseExpressionPtr evaluate3() const;

public:
    mutable hash_t _computed_hash;

    BaseExpressionPtr _head;
    Slice _leaves;  // other options: ropes, skip lists, ...

    inline Expression(BaseExpressionPtr head, const Slice &leaves) :
        _head(head->ref()),
        _leaves(leaves),
        _computed_hash(0) {
    }

    inline Expression(BaseExpressionPtr head, std::initializer_list<BaseExpressionPtr> leaves) :
        _head(head->ref()),
        _leaves(leaves.begin(), leaves.size()),
        _computed_hash(0) {
    }

    inline Expression(BaseExpressionPtr head, const std::vector<BaseExpressionPtr> &leaves) :
        _head(head->ref()),
        _leaves(&leaves[0], leaves.size()),
        _computed_hash(0) {
    }

    virtual ~Expression() {
        _head->deref();
    }

    virtual Type type() const {
        return ExpressionType;
    }

    virtual bool same(const BaseExpression *item) const;

    virtual hash_t hash() const;

    virtual std::string fullform() const;

    virtual BaseExpressionPtr evaluate() const;

    virtual Slice get_sequence() const {
        if (_head->is_symbol_sequence()) {
            return Slice(_leaves);
        } else {
            return Slice(this);
        }
    }

    virtual match_sizes_t match_num_args() const {
        return _head->match_num_args_with_head(this);
    }

    virtual Match match(
        Definitions *definitions,
        const BaseExpression *item) const;

    virtual Match match_sequence(
        const Matcher &matcher) const;
};


#endif
