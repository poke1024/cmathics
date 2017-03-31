#include "logic.h"
#include "../arithmetic/unary.h"

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

	virtual const char *operator_name() const {
		return "!";
	}

	virtual int precedence() const {
		return 230;
	}
};

void Builtins::Logic::initialize() {
	add<Not>();
}
