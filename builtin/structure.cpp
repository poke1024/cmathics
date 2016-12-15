#include "structure.h"

class Head : public Builtin {
public:
    static constexpr const char *name = "Head";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&Head::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        return BaseExpressionRef(expr->head(evaluation));
    }
};

void Builtins::Structure::initialize() {
    add<Head>();
}
