#include "logic.h"

class Not : public Builtin {
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
    #> !b
     = !b
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("Not[True]", "False");
		builtin("Not[False]", "True");
		builtin("Not[Not[expr_]]", "expr");
	}
};

void Builtins::Logic::initialize() {
	add<Not>();
}
