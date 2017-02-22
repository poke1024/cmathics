#include "types.h"
#include "expression.h"
#include "generator.h"
#include "numberform.h"
#include "string.h"
#include "evaluation.h"

inline StringRef round(StringRef number, machine_integer_t n_digits) {
    assert(n_digits < 0);
    const char *s = number->ascii();

    assert(s);
    mpz_class n(s);
    assert(n >= 0);

    mpz_class scale;
    mpz_ui_pow_ui(scale.get_mpz_t(), 10, -(1 + n_digits));

    n += 5 * scale;
    n /= 10 * scale;

    return Pool::String(n.get_str());
}

inline machine_integer_t round_exp(machine_integer_t exp, machine_integer_t step) {
    if (exp > 0) {
        return (exp / step) * step;
    } else {
        return ((exp / step) - 1) * step;
    }
}

class IllegalDigitBlock {
};

inline machine_integer_t digit_block(const BaseExpressionRef &rhs) {
    if (rhs->symbol() == S::Infinity) {
        return 0;
    } else {
        const auto int_value = rhs->get_machine_int_value();

        if (int_value && *int_value > 0) {
            return *int_value;
        }
    }

    throw IllegalDigitBlock();
}


inline StringRef NumberFormatter::blocks(
    StringPtr s,
    machine_integer_t start,
    machine_integer_t step,
    StringPtr separator) const {

    TempVector strings;

    if (start > 0) {
        strings.push_back(s->substr(0, start));
    }

    for (machine_integer_t i = start; i < s->length(); i += step) {
        if (!strings.empty()) {
            strings.push_back(separator);
        }
        strings.push_back(s->substr(i, i + step));
    }

    return string_array_join(strings);
}

inline BaseExpressionRef NumberFormatter::default_number_format(
    const BaseExpressionRef &man,
    const BaseExpressionRef &base,
    const BaseExpressionRef &exp,
    const NumberFormOptions &options,
    const BaseExpressionPtr form,
    const Evaluation &evaluation) const {

    if (exp->is_string() && exp->as_string()->length() > 0) {
        switch (form->symbol()) {
            case S::InputForm:
            case S::OutputForm:
            case S::FullForm:
                return expression(evaluation.RowBox,
                    expression(evaluation.List, man, m_mul_exp, exp));
            default:
                return expression(evaluation.RowBox, expression(
                    evaluation.List, man, options.NumberMultiplier,
                    expression(evaluation.SuperscriptBox, base, exp)));
        }
    } else {
        return man;
    }
}

void NumberFormatter::parse_option(
	NumberFormOptions &options,
    const NumberFormOptions &defaults,
	const SymbolPtr lhs,
	const BaseExpressionRef &rhs,
    const Evaluation &evaluation) const {

	const auto extract_string_pair =
        [this, &rhs, &options, &evaluation] (StringPtr *ptr, const char *error) {
            if (rhs->has_form(S::List, 2, evaluation)) {
                const auto * const leaves =
                    rhs->as_expression()->n_leaves<2>();

                for (int i = 0; i < 2; i++) {
                    const BaseExpressionRef &leaf = leaves[i];
                    if (leaf->is_string()) {
                        ptr[i] = leaf->as_string();
                    }
                }
            } else if (rhs->is_string()) {
                ptr[0] = rhs->as_string();
                ptr[1] = rhs->as_string();
            } else {
                evaluation.message(m_number_form, error, rhs);
                options.valid = false;
            }
        };

	switch (lhs->symbol()) {
		case S::NumberSigns:
			extract_string_pair(options.NumberSigns, "nsgn");
			break;

		case S::ExponentStep: {
			const optional<machine_integer_t> value =
                rhs->get_machine_int_value();
			if (value && *value > 0) {
				options.ExponentStep = *value;
			} else {
                evaluation.message(m_number_form, "estep", Pool::String("ExponentStep"), rhs);
                options.valid = false;
            }
			break;
		}

		case S::ExponentFunction:
			options.ExponentFunction = rhs.get();
			break;

		case S::DigitBlock:
            try {
                if (rhs->is_expression() &&
                    rhs->as_expression()->size() == 2) {

                    const auto *const leaves =
                        rhs->as_expression()->n_leaves<2>();
                    machine_integer_t block[2];

                    for (int i = 0; i < 2; i++) {
                        block[i] = digit_block(leaves[i]);
                    }

                    for (int i = 0; i < 2; i++) {
                        options.DigitBlock[i] = block[i];
                    }
                } else {
                    const auto value = digit_block(rhs);
                    options.DigitBlock[0] = value;
                    options.DigitBlock[1] = value;
                }
            } catch (const IllegalDigitBlock &) {
                evaluation.message(m_number_form, "dblk", rhs);
                options.valid = false;
            }
			break;

		case S::NumberSeparator: {
			extract_string_pair(options.NumberSeparator, "nspr");
			break;
		}

		case S::NumberPadding: {
			extract_string_pair(options.NumberPadding, "npad");
			break;
		}

		case S::SignPadding:
            switch (rhs->symbol()) {
                case S::True:
                    options.SignPadding = true;
                    break;
                case S::False:
                    options.SignPadding = false;
                    break;
                default:
                    evaluation.message(m_number_form, "opttf", rhs);
                    options.valid = false;
                    break;
            }
			break;

		case S::NumberPoint:
			if (rhs->is_string()) {
				options.NumberPoint = rhs->as_string();
			} else {
                evaluation.message(m_number_form, "npt", Pool::String("NumberPoint"), rhs);
                options.valid = false;
            }
			break;

		case S::NumberFormat:
            if (rhs->symbol() != S::Automatic) {
                const BaseExpressionPtr number_format = rhs.get();

                options.NumberFormat = [number_format] (
                    const NumberFormatter *formatter,
                    const BaseExpressionRef &man,
                    const BaseExpressionRef &base,
                    const BaseExpressionRef &exp,
                    const NumberFormOptions &options,
                    const BaseExpressionPtr form,
                    const Evaluation &evaluation) {

                    return expression(
                        number_format, man, base, exp, /*options_list*/ evaluation.empty_list);
                };
            } else {
                options.NumberFormat = defaults.NumberFormat;
            }
            break;

		case S::NumberMultiplier:
            if (rhs->is_string()) {
                options.NumberMultiplier = rhs->as_string();
            } else {
                evaluation.message(m_number_form, "npt", Pool::String("NumberMultiplier"), rhs);
                options.valid = false;
            }
			break;

		default:
			break; // ignore
	}
}

void NumberFormatter::parse_options(
    const BaseExpressionRef &options_list,
	NumberFormOptions &options,
    const NumberFormOptions &defaults,
    const Evaluation &evaluation) const {

    // IMPORTANT: note that "options" consists of un-refcounted pointers and is
    // only valid as long as "options_list" is valid!

    options = defaults;

    if (!options_list->is_expression()) {
        return;
    }

    options_list->as_expression()->with_slice([this, &options, &defaults, &evaluation] (const auto &slice) {
        const size_t n = slice.size();
        for (size_t i = 0; i < n; i++) {
            const BaseExpressionRef leaf = slice[i];
            if (leaf->has_form(S::Rule, 2, evaluation)) {
                const auto * const leaves =
                    leaf->as_expression()->n_leaves<2>();

                const BaseExpressionRef &lhs = leaves[0];
	            if (lhs->is_symbol()) {
		            const BaseExpressionRef &rhs = leaves[1];
		            parse_option(options, defaults, lhs->as_symbol(), rhs, evaluation);
	            }
            }
        }
    });
}

void NumberFormatter::parse_options(
	const OptionsMap &options_map,
	NumberFormOptions &options,
    const NumberFormOptions &defaults,
    const Evaluation &evaluation) const {

    options = defaults;

	for (auto i = options_map.begin(); i != options_map.end(); i++) {
		parse_option(options, defaults, i->first.get(), i->second, evaluation);
	}
}

NumberFormatter::NumberFormatter(const Symbols &symbols) {
    m_number_form = symbols.NumberForm;

    m_base_10 = Pool::MachineInteger(10);
    m_zero_digit = Pool::String("0");
    m_empty_string = Pool::String("");
    m_mul_exp = Pool::String("*^");

    const BaseExpressionPtr automatic = symbols.Automatic;

    m_number_signs[0] = Pool::String("-");
    m_number_signs[1] = Pool::String("");
    m_default_options.NumberSigns[0] = m_number_signs[0].get();
    m_default_options.NumberSigns[1] = m_number_signs[1].get();
    m_default_options.ExponentStep = 1;
    m_default_options.ExponentFunction = automatic;
    m_default_options.DigitBlock[0] = 0;
    m_default_options.DigitBlock[1] = 0;

    m_number_separator[0] = Pool::String(",");
    m_number_separator[1] = Pool::String(" ");
    m_default_options.NumberSeparator[0] = m_number_separator[0].get();
    m_default_options.NumberSeparator[1] = m_number_separator[1].get();

    m_number_padding[0] = Pool::String("");
    m_number_padding[1] = Pool::String("0");
    m_default_options.NumberPadding[0] = m_number_padding[0].get();
    m_default_options.NumberPadding[1] = m_number_padding[1].get();

    m_default_options.SignPadding = false;

    m_number_point = Pool::String(".");
    m_default_options.NumberPoint = m_number_point.get();

    m_default_options.NumberFormat = [] (
        const NumberFormatter *formatter,
        const BaseExpressionRef &man,
        const BaseExpressionRef &base,
        const BaseExpressionRef &exp,
        const NumberFormOptions &options,
        const BaseExpressionPtr form,
        const Evaluation &evaluation) -> BaseExpressionRef {

        if (exp->is_string() && exp->as_string()->length() > 0) {
            return expression(evaluation.RowBox, expression(
                evaluation.List, man, options.NumberMultiplier,
                expression(evaluation.SuperscriptBox, base, exp)));
        } else {
            return man;
        }
    };

    m_number_multiplier = Pool::String(std::string("\xC3\x97")); // i.e. \u00d7
    m_default_options.NumberMultiplier = m_number_multiplier.get();

    m_default_options.valid = true;

    m_make_boxes_default_options = m_default_options;

    m_make_boxes_default_options.DigitBlock[0] = 0;
    m_make_boxes_default_options.DigitBlock[1] = 0;
    m_make_boxes_default_options.ExponentFunction = automatic;
    m_make_boxes_default_options.ExponentStep = 1;
    m_make_boxes_default_options.NumberPadding[0] = m_number_padding[0].get();
    m_make_boxes_default_options.NumberPadding[1] = m_number_padding[1].get();
    m_make_boxes_default_options.NumberPoint = m_number_point.get();
    m_make_boxes_default_options.NumberSigns[0] = m_number_signs[0].get();
    m_make_boxes_default_options.NumberSigns[1] = m_number_signs[1].get();
    m_make_boxes_default_options.SignPadding = false;
    m_make_boxes_default_options.NumberMultiplier = m_number_multiplier.get();

    m_make_boxes_default_options.NumberFormat = [] (
        const NumberFormatter *formatter,
        const BaseExpressionRef &man,
        const BaseExpressionRef &base,
        const BaseExpressionRef &exp,
        const NumberFormOptions &options,
        const BaseExpressionPtr form,
        const Evaluation &evaluation) -> BaseExpressionRef {

        if (exp->is_string() && exp->as_string()->length() > 0) {
            switch (form->symbol()) {
                case S::InputForm:
                case S::OutputForm:
                case S::FullForm:
                    return expression(evaluation.RowBox,
                        expression(evaluation.List, man, formatter->m_mul_exp, exp));
                default:
                    return expression(evaluation.RowBox, expression(
                        evaluation.List, man, options.NumberMultiplier,
                        expression(evaluation.SuperscriptBox, base, exp)));
            }
        } else {
            return man;
        }
    };
}

BaseExpressionRef NumberFormatter::operator()(
    const SExp &s_exp,
    const machine_integer_t n,
    const optional<machine_integer_t> f,
    BaseExpressionPtr form,
	const NumberFormOptions &options,
    const Evaluation &evaluation) const {

    UnsafeStringRef s;
    machine_integer_t exp;
    int non_negative;
    bool is_int_type;

    std::tie(s, exp, non_negative, is_int_type) = s_exp;

    bool is_int = false;
    if (is_int_type) {
        if (!f) {
            is_int = true;
        }
    }

    assert(n > 0);
    assert(non_negative >= 0 && non_negative <= 1);

    const StringPtr sign_prefix =
        options.NumberSigns[non_negative];

    const auto exp_step = options.ExponentStep;

    // round exponent to ExponentStep
    const machine_integer_t rexp = round_exp(exp, exp_step);

    UnsafeBaseExpressionRef pexp;
    if (is_int) {
        // integer never uses scientific notation
        pexp = m_empty_string;
    } else {
        optional<machine_integer_t> value;

        if (options.ExponentFunction->symbol() == S::Automatic) {
            if (rexp >= -5 && rexp <= 5) {
                // ignore
            } else {
                value = rexp;
            }
        } else {
            value = expression(
                options.ExponentFunction,
                from_primitive(rexp))->evaluate_or_copy(evaluation)->get_machine_int_value();
        }

        if (value) {
            exp -= *value;
            pexp = Pool::String(std::to_string(*value));
        } else {
            pexp = m_empty_string;
        }
    }

    // pad right with '0'.
    if (machine_integer_t(s->length()) < exp + 1) {
        evaluation.message(m_number_form, "sigz");
        s = string_join(s, m_zero_digit->repeat(1 + exp - s->length()));
    }

    // pad left with '0'.
    if (exp < 0) {
        s = string_join(m_zero_digit->repeat(-exp), s);
        exp = 0;
    }

    // left and right of NumberPoint
    UnsafeStringRef left = s->substr(0, exp + 1);
    UnsafeStringRef right = s->substr(exp + 1);

    // pad with NumberPadding
    if (f) {
        const size_t k = right->length();
        if (k < *f) { // pad right?
            right = string_join(right, options.NumberPadding[1]->repeat(*f - right->length()));
        } else if (k > *f) { // round right?
            const StringRef number = round(string_join(left, right), *f - k);
            left = number->substr(0, exp + 1);
            right = number->substr(exp + 1);
        }
    }

    // insert NumberSeparator
    const auto &digit_block = options.DigitBlock;

    if (digit_block[0]) {
        left = blocks(
            left.get(),
            left->length() % digit_block[0],
            digit_block[0],
            options.NumberSeparator[0]);
    }

    if (digit_block[1]) {
        right = blocks(
            right.get(),
            0,
            digit_block[1],
            options.NumberSeparator[1]);
    }

    const machine_integer_t max_sign_len = std::max(
        options.NumberSigns[0]->length(),
        options.NumberSigns[1]->length()
    );

    const machine_integer_t l =
        sign_prefix->length() +
        left->length() +
        right->length() -
        max_sign_len;

    machine_integer_t left_padding;

    if (l < n) {
        left_padding = n - l;
    } else if (sign_prefix->length() < max_sign_len) {
        left_padding = max_sign_len - sign_prefix->length();
    } else {
	    left_padding = 0;
    }
    assert(left_padding >= 0);

    const StringRef left_padding_string =
        options.NumberPadding[0]->repeat(left_padding);

    // insert NumberPoint
    UnsafeStringRef prefix;

    if (options.SignPadding) {
        prefix = string_join(sign_prefix, left_padding_string);
    } else {
        prefix = string_join(left_padding_string, sign_prefix);
    }

    if (is_int) {
        s = string_join(prefix, left);
    } else {
        s = string_join(prefix, left, options.NumberPoint, right);
    }

    return options.NumberFormat(this, s, m_base_10, pexp, options, form, evaluation);
}