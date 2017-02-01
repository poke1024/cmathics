#include "evaluation.h"

class HoldForm : public Builtin {
public:
    static constexpr const char *name = "HoldForm";

    static constexpr auto attributes = Attributes::HoldAll;

    static constexpr const char *docs = R"(
    <dl>
    <dt>'HoldForm[$expr$]'
        <dd>is equivalent to 'Hold[$expr$]', but prints as $expr$.
    </dl>

    >> HoldForm[1 + 2 + 3]
     = 1 + 2 + 3

    'HoldForm' has attribute 'HoldAll':
    >> Attributes[HoldForm]
     = {HoldAll, Protected}
    )";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        rule("MakeBoxes[HoldForm[expr_], f_]", "MakeBoxes[expr, f]");
    }
};

void Builtins::Evaluation::initialize() {
    add<HoldForm>();
}