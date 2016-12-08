#ifndef STRING_H
#define STRING_H

#include "types.h"
#include <string>
#include "hash.h"

class String : public BaseExpression {
private:
	mutable optional<SymbolRef> m_option_symbol; // "System`" + value

public:
    const std::string value;

    explicit String(const char *text) : BaseExpression(StringExtendedType), value(text) {
    }

    explicit String(const std::string &text) : BaseExpression(StringExtendedType), value(text) {
    }

	inline SymbolRef option_symbol(const Evaluation &evaluation) const;

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

    inline std::string str() const {
        return value;
    }

    inline const char *c_str() const {
        return value.c_str();
    }

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }
};

inline BaseExpressionRef from_primitive(const std::string &value) {
    return BaseExpressionRef(new String(value));
}

#endif
