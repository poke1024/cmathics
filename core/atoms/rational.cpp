#include "core/types.h"
#include "core/expression/implementation.h"
#include "rational.h"

std::string BigRational::debugform() const {
	std::ostringstream s;
	s << "Rational[" << value.get_num().get_str() << ", " << value.get_den().get_str() << "]";
	return s.str();
}

BaseExpressionRef BigRational::custom_format(
	const BaseExpressionRef &form,
	const Evaluation &evaluation) const {

    switch (form->symbol()) {
	    case S::FullForm:
			return expression(
				expression(evaluation.HoldForm, evaluation.Rational),
				from_primitive(value.get_num()),
				from_primitive(value.get_den()))->custom_format_or_copy(form, evaluation);

		default: {
			UnsafeBaseExpressionRef leaf;

			bool minus = mpz_sgn(value.get_num().get_mpz_t()) * mpz_sgn(value.get_den().get_mpz_t()) < 0;

			const auto numerator = abs(value.get_num());
			const auto denominator = abs(value.get_den());

			if (!minus) {
				leaf = expression(evaluation.Divide,
					BigInteger::construct(numerator), BigInteger::construct(denominator));
			} else {
				leaf = expression(evaluation.Minus, expression(evaluation.Divide,
					BigInteger::construct(-numerator), BigInteger::construct(denominator)));
			}

			return expression(evaluation.HoldForm, leaf)->custom_format_or_copy(form, evaluation);
		}
	}
}

BaseExpressionPtr BigRational::head(const Symbols &symbols) const {
    return symbols.Rational;
}

BaseExpressionRef BigRational::negate(const Evaluation &evaluation) const {
    return from_primitive(-value);
}
