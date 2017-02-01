#include "types.h"
#include "expression.h"
#include "rational.h"

BaseExpressionRef BigRational::custom_format(
	const BaseExpressionRef &form,
	const Evaluation &evaluation) const {

    switch (form->extended_type()) {
		case SymbolFullForm:
			return expression(
				expression(evaluation.HoldForm, evaluation.Rational),
				from_primitive(value.get_num()),
				from_primitive(value.get_den()))->custom_format(form, evaluation);

		default: {
			UnsafeBaseExpressionRef leaf;

			bool minus = mpz_sgn(value.get_num().get_mpz_t()) * mpz_sgn(value.get_den().get_mpz_t()) < 0;

			const auto numerator = abs(value.get_num());
			const auto denominator = abs(value.get_den());

			if (!minus) {
				leaf = expression(evaluation.Divide,
					Pool::BigInteger(numerator), Pool::BigInteger(denominator));
			} else {
				leaf = expression(evaluation.Minus, expression(evaluation.Divide,
					Pool::BigInteger(-numerator), Pool::BigInteger(denominator)));
			}

			return expression(evaluation.HoldForm, leaf)->custom_format(form, evaluation);
		}
	}
}

BaseExpressionPtr BigRational::head(const Symbols &symbols) const {
    return symbols.Rational;
}
