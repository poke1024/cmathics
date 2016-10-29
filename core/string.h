#ifndef STRING_H
#define STRING_H

#include "types.h"
#include <string>
#include "hash.h"

class String : public BaseExpression {
public:
    const std::string value;

    String(const char *new_value) : value(new_value) {
    }

    virtual Type type() const {
        return StringType;
    }

    virtual bool same(const BaseExpression *expr) const {
        if (expr->type() == StringType) {
            return value == static_cast<const String*>(expr)->value;
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

    virtual Match match(const BaseExpression *expr) const {
        return Match(same(expr));
    }
};

inline BaseExpressionRef string(const char *value) {
    return std::make_shared<String>(value);
}

#endif
