#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "types.h"
#include "complex.h"
#include "expression.h"

const std::hash<machine_real_t> MachineComplex::hash_function = std::hash<machine_real_t>();

BaseExpressionRef MachineComplex::custom_format(
	const BaseExpressionRef &form,
	const Evaluation &evaluation) const {

	switch (form->extended_type()) {
		case SymbolFullForm:
			return expression(
				expression(evaluation.HoldForm, evaluation.Complex),
				Pool::MachineReal(value.real()),
				Pool::MachineReal(value.imag()))->custom_format(form, evaluation);

		default: {
			UnsafeBaseExpressionRef leaf;

			const machine_real_t real = value.real();
			const machine_real_t imag = value.imag();

			if (real) {
				if (imag == 1.0) {
					leaf = expression(evaluation.Plus, Pool::MachineReal(real), evaluation.I);
				} else {
					leaf = expression(evaluation.Plus, Pool::MachineReal(real),
						expression(evaluation.Times, Pool::MachineReal(imag), evaluation.I));
				}
			} else {
				if (imag == 1.0) {
					leaf = evaluation.I;
				} else {
					leaf = expression(evaluation.Times, Pool::MachineReal(imag), evaluation.I);
				}
			}

			return expression(evaluation.HoldForm, leaf)->custom_format(form, evaluation);
		}
	}
}

BaseExpressionPtr MachineComplex::head(const Symbols &symbols) const {
    return symbols.Complex;
}

BaseExpressionRef BigComplex::custom_format(
	const BaseExpressionRef &form,
	const Evaluation &evaluation) const {

	switch (form->extended_type()) {
		case SymbolFullForm:
			return expression(
				expression(evaluation.HoldForm, evaluation.Complex),
				Pool::String(m_value->real_part()->__str__()),
				Pool::String(m_value->imaginary_part()->__str__()))->custom_format(form, evaluation);

		default: {
			UnsafeBaseExpressionRef leaf;

			const auto real = m_value->real_part();
			const auto imag = m_value->imaginary_part();

			mpq_class real_mpq(m_value->real_.get_mpq_t());
			mpq_class imag_mpq(m_value->imaginary_.get_mpq_t());

			if (!real->is_zero()) {
				if (imag->is_one()) {
					leaf = expression(evaluation.Plus, Pool::BigRational(real_mpq), evaluation.I);
				} else {
					leaf = expression(evaluation.Plus, Pool::BigRational(real_mpq),
						expression(evaluation.Times, Pool::BigRational(imag_mpq), evaluation.I));
				}
			} else {
				if (imag->is_one()) {
					leaf = evaluation.I;
				} else {
					leaf = expression(evaluation.Times, Pool::BigRational(imag_mpq), evaluation.I);
				}
			}

			return expression(evaluation.HoldForm, leaf)->custom_format(form, evaluation);
		}
	}
}

BaseExpressionPtr BigComplex::head(const Symbols &symbols) const {
    return symbols.Complex;
}

