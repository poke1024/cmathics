#include "types.h"
#include "expression.h"
#include "rational.h"

BaseExpressionPtr BigRational::head(const Evaluation &evaluation) const {
    return evaluation.Rational;
}

std::string BigRational::format(const SymbolRef &form, const Evaluation &evaluation) const {
    switch (form->extended_type()) {
        case SymbolFullForm:
            return expression(
                expression(evaluation.HoldForm, evaluation.Rational),
                from_primitive(value.get_num()),
                from_primitive(value.get_den()))->format(form, evaluation);
        default: {
            std::ostringstream s;
            s << value.get_num().get_str() << " / " << value.get_den().get_str();
            return s.str();
        }
    }
}
