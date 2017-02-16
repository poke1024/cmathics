#ifndef CMATHICS_NUMBERFORM_H
#define CMATHICS_NUMBERFORM_H

class NumberFormOptions {
public:
    StringPtr NumberSigns[2];
    machine_integer_t ExponentStep;
    BaseExpressionPtr ExponentFunction;
    machine_integer_t DigitBlock[2];
    StringPtr NumberSeparator[2];
    StringPtr NumberPadding[2];
    bool SignPadding;
    StringPtr NumberPoint;
    BaseExpressionPtr NumberFormat;
    BaseExpressionPtr NumberMultiplier;
};

class Runtime;

class NumberFormatter {
private:
    UnsafeSymbolRef m_number_form;

    UnsafeBaseExpressionRef m_base_10;
    UnsafeStringRef m_zero_digit;
    UnsafeStringRef m_empty_string;
    UnsafeStringRef m_mul_exp;

    UnsafeStringRef m_number_signs[2];
    UnsafeStringRef m_number_separator[2];
    UnsafeStringRef m_number_padding[2];
    UnsafeStringRef m_number_point;
    UnsafeStringRef m_number_multiplier;

	NumberFormOptions m_default_options;

public:
	NumberFormatter(const Symbols &symbols);

	void parse_options(
		const OptionsMap &options_map,
		NumberFormOptions &options,
		const Evaluation &evaluation) const;

	void parse_options(
		const BaseExpressionRef &options_list,
			NumberFormOptions &options,
		const Evaluation &evaluation) const;

private:
	inline void init_options(NumberFormOptions &options) const {
		std::memcpy(&options, &m_default_options, sizeof(NumberFormOptions));
	}

	inline void parse_option(
		NumberFormOptions &options,
		const BaseExpressionRef &lhs,
		const BaseExpressionRef &rhs) const;

    inline StringRef blocks(
        StringPtr s,
        machine_integer_t start,
        machine_integer_t step,
        StringPtr separator) const;

    BaseExpressionRef default_number_format(
        const BaseExpressionRef &man,
        const BaseExpressionRef &base,
        const BaseExpressionRef &exp,
        const NumberFormOptions &options,
        BaseExpressionPtr form,
        const Evaluation &evaluation) const;

public:
    BaseExpressionRef operator()(
        const SExp &s_exp,
        const size_t n,
        BaseExpressionPtr form,
	    const NumberFormOptions &options,
        const Evaluation &evaluation) const;
};

#endif //CMATHICS_NUMBERFORM_H
