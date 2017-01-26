#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "types.h"
#include "complex.h"
#include "expression.h"

const std::hash<machine_real_t> MachineComplex::hash_function = std::hash<machine_real_t>();

BaseExpressionPtr MachineComplex::head(const Evaluation &evaluation) const {
    return evaluation.Complex;
}

std::string MachineComplex::format(const SymbolRef &form, const Evaluation &evaluation) const {
    switch (form->extended_type()) {
        case SymbolFullForm:
            return expression(
                expression(evaluation.HoldForm, evaluation.Complex),
                Pool::MachineReal(value.real()),
                Pool::MachineReal(value.imag()))->format(form, evaluation);
        default: {
            std::ostringstream s;
            const machine_real_t imag = value.imag();
            s << std::showpoint << std::setprecision(6);
            if (value.real() != 0.0) {
                s << value.real() << (imag >= 0. ? " + " : " - ") << std::abs(imag) << " I";
            } else {
                s << imag << " I";
            }
            return s.str();
        }
    }
}

BaseExpressionPtr BigComplex::head(const Evaluation &evaluation) const {
    return evaluation.Complex;
}

std::string BigComplex::format(const SymbolRef &form, const Evaluation &evaluation) const {
    switch (form->extended_type()) {
        case SymbolFullForm:
            return expression(
                expression(evaluation.HoldForm, evaluation.Complex),
                Pool::String(m_value->real_part()->__str__()),
                Pool::String(m_value->imaginary_part()->__str__()))->format(form, evaluation);
        default: {
            std::ostringstream s;
            const auto real = m_value->real_part();
            const auto imag = m_value->imaginary_part();
            if (!real->is_zero()) {
                s << real->__str__();

	            if (imag->is_one()) {
		            s << " + I";
	            } else if (imag->is_minus_one()) {
		            s << " - I";
	            } else {
		            s << (imag->is_negative() ? " - " : " + ");
		            s << imag->__str__() << " I";
	            }
            } else {
                if (imag->is_one()) {
	                s << "I";
                } else if (imag->is_minus_one()) {
	                s << "-I";
                } else {
	                s << imag->__str__() << " I";
                }
            }
            return s.str();
        }
    }
}
