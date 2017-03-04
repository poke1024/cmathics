#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "core/types.h"
#include "complex.h"
#include "core/expression/implementation.h"

const std::hash<machine_real_t> MachineComplex::hash_function = std::hash<machine_real_t>();

std::string MachineComplex::debugform() const {
	std::ostringstream s;
	s << "Complex[" << m_value.real() << ", " << m_value.imag() << "]";
	return s.str();
}

BaseExpressionRef MachineComplex::custom_format(
	const BaseExpressionRef &form,
	const Evaluation &evaluation) const {

	switch (form->symbol()) {
		case S::FullForm:
			return expression(
				expression(evaluation.HoldForm, evaluation.Complex),
				MachineReal::construct(m_value.real()),
				MachineReal::construct(m_value.imag()))->custom_format_or_copy(form, evaluation);

		default: {
			UnsafeBaseExpressionRef leaf;

			const machine_real_t real = m_value.real();
			const machine_real_t imag = m_value.imag();

			if (real) {
				if (imag == 1.0) {
					leaf = expression(evaluation.Plus, MachineReal::construct(real), evaluation.I);
				} else {
					leaf = expression(evaluation.Plus, MachineReal::construct(real),
						expression(evaluation.Times, MachineReal::construct(imag), evaluation.I));
				}
			} else {
				if (imag == 1.0) {
					leaf = evaluation.I;
				} else {
					leaf = expression(evaluation.Times, MachineReal::construct(imag), evaluation.I);
				}
			}

			return expression(evaluation.HoldForm, leaf)->custom_format_or_copy(form, evaluation);
		}
	}
}

BaseExpressionPtr MachineComplex::head(const Symbols &symbols) const {
    return symbols.Complex;
}

BaseExpressionRef MachineComplex::negate(const Evaluation &evaluation) const {
    return MachineComplex::construct(-m_value.real(), -m_value.imag());
}

void MachineComplex::sort_key(SortKey &key, const Evaluation &evaluation) const {
	m_components.lock([this, &key] (auto &components) {
		if (!components) {
			components.emplace(ComplexComponents{
				MachineReal::construct(m_value.real()),
				MachineReal::construct(m_value.imag())
			});
		}

		key.construct(
			0,
			0,
			BaseExpressionPtr(components->real.get()),
			BaseExpressionPtr(components->imag.get()),
			1);

		return nothing();
	});
}

std::string BigComplex::debugform() const {
	std::ostringstream s;
	s << "Complex[" << m_value->real_part()->__str__() << ", " << m_value->imaginary_part()->__str__() << "]";
	return s.str();
}

BaseExpressionRef BigComplex::custom_format(
	const BaseExpressionRef &form,
	const Evaluation &evaluation) const {

	switch (form->symbol()) {
		case S::FullForm:
			return expression(
				expression(evaluation.HoldForm, evaluation.Complex),
				String::construct(m_value->real_part()->__str__()),
				String::construct(m_value->imaginary_part()->__str__()))->custom_format_or_copy(form, evaluation);

		default: {
			UnsafeBaseExpressionRef leaf;

			const auto real = m_value->real_part();
			const auto imag = m_value->imaginary_part();

			mpq_class real_mpq(m_value->real_.get_mpq_t());
			mpq_class imag_mpq(m_value->imaginary_.get_mpq_t());

			if (!real->is_zero()) {
				if (imag->is_one()) {
					leaf = expression(evaluation.Plus, from_primitive(real_mpq), evaluation.I);
				} else {
					leaf = expression(evaluation.Plus, from_primitive(real_mpq),
						expression(evaluation.Times, from_primitive(imag_mpq), evaluation.I));
				}
			} else {
				if (imag->is_one()) {
					leaf = evaluation.I;
				} else {
					leaf = expression(evaluation.Times, from_primitive(imag_mpq), evaluation.I);
				}
			}

			return expression(evaluation.HoldForm, leaf)->custom_format_or_copy(form, evaluation);
		}
	}
}

BaseExpressionPtr BigComplex::head(const Symbols &symbols) const {
    return symbols.Complex;
}

BaseExpressionRef BigComplex::negate(const Evaluation &evaluation) const {
    const SymEngine::Integer &minus_one = *SymEngine::minus_one;

    SymEngine::RCP<const SymEngine::Number> real = m_value->real_part()->mul(minus_one);
    SymEngine::RCP<const SymEngine::Number> imag = m_value->imaginary_part()->mul(minus_one);

    return BigComplex::construct(SymEngine::rcp_static_cast<const SymEngine::Complex>(
        SymEngine::Complex::from_two_nums(*real, *imag)));
}

void BigComplex::sort_key(SortKey &key, const Evaluation &evaluation) const {
	m_components.lock([this, &key, &evaluation] (auto &components) {
		if (!components) {
			components.emplace(ComplexComponents{
				from_symbolic_form(m_value->real_part(), evaluation),
				from_symbolic_form(m_value->imaginary_part(), evaluation)
			});
		}

		key.construct(
			0,
			0,
			BaseExpressionPtr(components->real.get()),
			BaseExpressionPtr(components->imag.get()),
			1);

		return nothing();
	});
}