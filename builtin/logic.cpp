#include "logic.h"

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
				std::string form = string_format(
					"%s[{HoldForm[item]},\"%s\",%d]", format_function(), operator_(), precedence());

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

	virtual const char *operator_() const = 0;

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

class Not : public PrefixOperator {
public:
	static constexpr const char *name = "Not";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Not[$expr$]'
    <dt>'!$expr$'
        <dd>negates the logical expression $expr$.
    </dl>

    >> !True
     = False
    >> !False
     = True
    >> !b
     = !b
	)";

public:
	using PrefixOperator::PrefixOperator;

	void build(Runtime &runtime) {
		builtin("Not[True]", "False");
		builtin("Not[False]", "True");
		builtin("Not[Not[expr_]]", "expr");
		add_operator_formats();
	}

	virtual const char *operator_() const {
		return "!";
	}

	virtual int precedence() const {
		return 230;
	}
};

void Builtins::Logic::initialize() {
	add<Not>();
}
