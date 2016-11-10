#ifndef STRING_H
#define STRING_H

#include "types.h"
#include <string>
#include "hash.h"

class String : public BaseExpression {
public:
    const std::string value;

    explicit String(const std::string &new_value) : BaseExpression(StringType), value(new_value) {
    }

    virtual bool same(const BaseExpression &expr) const {
        if (expr.type() == StringType) {
            return value == static_cast<const String*>(&expr)->value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        return hash_pair(string_hash, djb2(value.c_str()));
    }

    virtual std::string fullform() const {
        return value;
    }

    virtual const char *get_string_value() const {
        return value.c_str();
    }

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }
};

inline BaseExpressionRef from_primitive(const std::string &value) {
    return std::make_shared<String>(value);
}

template<typename Alloc>
inline BaseExpressionRef from_primitive(const Alloc& alloc, const std::string &value) {
    return std::allocate_shared<String, Alloc>(alloc, value);
}

#endif
