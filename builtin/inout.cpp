#include "inout.h"

class FullForm : public Builtin {
public:
	static constexpr const char *name = "FullForm";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'FullForm[$expr$]'
        <dd>displays the underlying form of $expr$.
    </dl>

    >> FullForm[a + b * c]
     = Plus[a, Times[b, c]]
    >> FullForm[2/3]
     = Rational[2, 3]
    >> FullForm["A string"]
     = "A string"
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		// FIXME. currently a noop.
		rule("FullForm[x_]", "x");
	}
};

void Builtins::InOut::initialize() {
	add<FullForm>();
}
