#include "types.h"
#include "expression.h"
#include "generator.h"
#include "numberform.h"
#include "string.h"
#include "evaluation.h"

inline StringRef NumberForm::blocks(
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

inline BaseExpressionRef NumberForm::default_number_format(
    const BaseExpressionRef &man,
    const BaseExpressionRef &base,
    const BaseExpressionRef &exp,
    const NumberFormatOptions &options,
    const Evaluation &evaluation) const {

    if (exp->is_string() && exp->as_string()->length() > 0) {
        const BaseExpressionPtr mul = options.NumberMultiplier;
        return expression(evaluation.RowBox, expression(
                evaluation.List, man, mul, expression(evaluation.SuperscriptBox, base, exp)));
    } else {
        return man;
    }
}

void NumberForm::parse_options(
    const BaseExpressionRef &options_list,
    NumberFormatOptions &options,
    const Evaluation &evaluation) const {

    // IMPORTANT: note that "options" consists of un-refcounted pointers and is
    // only valid as long as "options_list" is valid!

    std::memcpy(&options, &m_default_options, sizeof(NumberFormatOptions));

    if (!options_list->is_expression()) {
        return;
    }

    options_list->as_expression()->with_slice([&options, &evaluation] (const auto &slice) {
        const size_t n = slice.size();
        for (size_t i = 0; i < n; i++) {
            const BaseExpressionRef leaf = slice[i];
            if (leaf->has_form(S::Rule, 2, evaluation)) {
                const BaseExpressionRef * const leaves =
                        leaf->as_expression()->static_leaves<2>();
                const BaseExpressionRef &lhs = leaves[0];
                const BaseExpressionRef &rhs = leaves[1];

                const auto extract_string_pair = [&rhs] (StringPtr *ptr) {
                    if (rhs->is_expression() &&
                        rhs->as_expression()->size() == 2) {

                        const auto * const leaves =
                                rhs->as_expression()->static_leaves<2>();

                        for (int i = 0; i < 2; i++) {
                            const BaseExpressionRef &leaf = leaves[i];
                            if (leaf->is_string()) {
                                ptr[i] = leaf->as_string();
                            }
                        }
                    }
                };

                switch (lhs->symbol()) {
                    case S::NumberSigns:
                        extract_string_pair(options.NumberSigns);
                        break;

                    case S::ExponentStep: {
                        const optional<machine_integer_t> value =
                                rhs->get_int_value();
                        if (value) {
                            options.ExponentStep = *value;
                        }
                        break;
                    }

                    case S::ExponentFunction:
                        options.ExponentFunction = rhs.get();
                        break;

                    case S::DigitBlock:
                        if (rhs->is_expression() &&
                            rhs->as_expression()->size() == 2) {

                            const auto * const leaves =
                                    rhs->as_expression()->static_leaves<2>();

                            for (int i = 0; i < 2; i++) {
                                const auto value = leaves[i]->get_int_value();
                                options.DigitBlock[i] = value ? *value : 0;
                            }
                        }
                        break;

                    case S::NumberSeparator: {
                        extract_string_pair(options.NumberSeparator);
                        break;
                    }

                    case S::NumberPadding: {
                        extract_string_pair(options.NumberPadding);
                        break;
                    }

                    case S::SignPadding:
                        options.SignPadding = rhs->is_true();
                        break;

                    case S::NumberPoint:
                        if (rhs->is_string()) {
                            options.NumberPoint = rhs->as_string();
                        }
                        break;

                    case S::NumberFormat:
                        options.NumberFormat = rhs.get();
                        break;

                    case S::NumberMultiplier:
                        options.NumberMultiplier = rhs.get();
                        break;

                    default:
                        break; // ignore
                }
            }
        }
    });
}

NumberForm::NumberForm(const Symbols &symbols) {
    m_number_form = symbols.NumberForm;

    m_base_10 = Pool::MachineInteger(10);
    m_zero_digit = Pool::String("0");
    m_empty_string = Pool::String("");

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

    m_default_options.NumberFormat = automatic;

    m_number_multiplier = Pool::String(std::string("\x00\xd7"));
    m_default_options.NumberMultiplier = m_number_multiplier.get();
}

BaseExpressionRef NumberForm::operator()(
    const SExp &s_exp,
    const BaseExpressionRef &options_list,
    const size_t n,
    const Evaluation &evaluation) const {

    NumberFormatOptions options;

    parse_options(options_list, options, evaluation);

    const bool is_int = false;

    UnsafeStringRef s;
    machine_integer_t exp;
    int non_negative;

    std::tie(s, exp, non_negative) = s_exp;

    assert(n > 0);
    assert(non_negative >= 0 && non_negative <= 1);

    const StringPtr sign_prefix =
        options.NumberSigns[non_negative];

    const auto exp_step = options.ExponentStep;

    // round exponent to ExponentStep
    const machine_integer_t rexp = (exp / exp_step) * exp_step;

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
                from_primitive(rexp))->evaluate(evaluation)->get_int_value();
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
    // FIXME

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

    machine_integer_t max_sign_len = std::max(
        options.NumberSigns[0]->length(),
        options.NumberSigns[1]->length()
    );

    machine_integer_t l =
        sign_prefix->length() +
        left->length() +
        right->length() -
        max_sign_len;

    machine_integer_t left_padding;

    if (l < n) {
        left_padding = n - l;
    } else if (sign_prefix->length() < max_sign_len) {
        left_padding = max_sign_len - sign_prefix->length();
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

    const BaseExpressionPtr NumberFormat = options.NumberFormat;

    if (NumberFormat->symbol() != S::Automatic) {
        return expression(NumberFormat, s, m_base_10, pexp, options_list);
    } else {
        return default_number_format(s, m_base_10, pexp, options, evaluation);
    }
}