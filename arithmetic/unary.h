#pragma once

class UnaryOperator : public Builtin {
protected:
    void add_operator_formats() {
        if (default_formats()) {
            std::string name = m_symbol->name();

            if (needs_verbatim()) {
                name = std::string("Verbatim[") + name + "]";
            }

            std::string pattern = name + "[item_]";

            if (!has_format(pattern.c_str())) {
                const std::string form = string_format(
                    "%s[{HoldForm[item]},\"%s\",%d]", format_function(), operator_name(), precedence());

                format(pattern.c_str(), form.c_str());
            }
        }
    }

protected:
    virtual const char *format_function() const = 0;

public:
    using Builtin::Builtin;

    virtual bool needs_verbatim() const {
        return false;
    }

    virtual bool default_formats() const {
        return true;
    }

    virtual const char *operator_name() const = 0;

    virtual int precedence() const = 0;
};

class PrefixOperator : public UnaryOperator {
protected:
    virtual const char *format_function() const {
        return "Prefix";
    }

public:
    using UnaryOperator::UnaryOperator;
};

class PostfixOperator : public UnaryOperator {
protected:
    virtual const char *format_function() const {
        return "Postfix";
    }

public:
    using UnaryOperator::UnaryOperator;
};
